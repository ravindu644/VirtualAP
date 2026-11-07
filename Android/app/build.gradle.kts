import java.security.MessageDigest

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

// One version for the app and the backend it ships: VAP_VERSION from the C header.
val vapVersionName = Regex("#define\\s+VAP_VERSION\\s+\"([^\"]+)\"")
    .find(rootProject.file("../src/include/virtualap.h").readText())?.groupValues?.get(1)
    ?: error("VAP_VERSION not found in src/include/virtualap.h")
// Every commit gets its own build number, so CI APKs between version bumps
// still install over each other. Needs full history: a shallow clone counts 1,
// and a tarball without .git counts 0. Both still sort above the old scheme.
val gitCommitCount = runCatching {
    providers.exec { commandLine("git", "rev-list", "--count", "HEAD") }
        .standardOutput.asText.get().trim().toInt()
}.getOrDefault(0)
val vapReleaseCode = vapVersionName.split(".").let { parts ->
    require(parts.size == 3 && parts.all { it.toIntOrNull() != null }) { "VAP_VERSION must be X.Y.Z, got '$vapVersionName'" }
    parts[0].toInt() * 10000 + parts[1].toInt() * 100 + parts[2].toInt()
}
// ponytail: the suffix is the repository's total commit count, so past 10000 commits a
// release's codes reach into the next one's, and major 21 overflows Int. A rewritten
// history counts lower and the APK becomes a downgrade.
val vapVersionCode = vapReleaseCode * 10000 + gitCommitCount

android {
    namespace = "com.virtualap.app"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.virtualap.app"
        minSdk = 26
        targetSdk = 34
        versionCode = vapVersionCode
        versionName = vapVersionName
        // No native libs: the static binaries ship as assets and the app deploys the
        // device's arch, so one APK installs on arm64-v8a and armeabi-v7a alike.
    }

    signingConfigs {
        // virtualap.keystore signs debug and release alike. The passwords come
        // from -P, gradle.properties or ~/.gradle/gradle.properties.
        val keystoreFile = rootProject.file("../virtualap.keystore")
        // CI passes every -P, set or not, so an empty value counts as unset.
        fun prop(name: String) = providers.gradleProperty(name).orNull?.takeIf { it.isNotEmpty() }
        val keystorePassword = prop("KEYSTORE_PASSWORD")
        if (keystoreFile.exists() && keystorePassword != null) {
            getByName("debug") {
                storeFile = keystoreFile
                storePassword = keystorePassword
                keyAlias = prop("KEY_ALIAS") ?: "virtualap"
                keyPassword = prop("KEY_PASSWORD") ?: keystorePassword
                enableV2Signing = true
                enableV3Signing = true
            }
            create("release") { initWith(getByName("debug")) }
        }
        // No release key (a fork, a pull request): the release build type signs with
        // the debug config below. AGP creates ~/.android/debug.keystore when that
        // config's file is missing, as on a fresh runner, but only for the config
        // named "debug", not for a copy of it under another name.
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            signingConfig = signingConfigs.findByName("release") ?: signingConfigs.getByName("debug")
        }
        debug {
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        compose = true
    }

    // res/xml/_generated_res_locale_config.xml, from the values-* folders Weblate adds and
    // res/resources.properties' unqualifiedResLocale. AGP wires it into the manifest for
    // Android 13's per-app language list, and LocaleHelper reads it for the picker.
    androidResources {
        generateLocaleConfig = true
    }

    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
    }
}

// The static binaries, staged into a generated assets root so
// nothing the build writes shows up as a diff. Each arch carries PAYLOAD_VERSION,
// a hash of its binaries and boot module; the app compares it to its last deploy to know when an
// APK update ships new ones.
val backendDir = rootProject.file("../backend")
val arches = listOf("aarch64", "armhf")
// The payload, by name: backend/<arch> may still hold binaries an older build
// left there, which nothing runs any more.
val payload = listOf("virtualap", "hostapd", "dnsmasq")
val assetsDir = layout.buildDirectory.dir("generated/assets")
android.sourceSets["main"].assets.srcDir(assetsDir)

// The engine the APK ships. make is incremental and knows its own inputs, so an unchanged src/
// costs one no-op per arch, and an APK can never carry an engine older than src/.
val buildBackend = tasks.register<Exec>("buildBackend") {
    workingDir(rootProject.file(".."))
    commandLine(listOf("make") + arches)
}

val prepareAssets = tasks.register<Sync>("prepareAssets") {
    dependsOn(buildBackend)
    for (arch in arches) from(File(backendDir, arch)) { into("bin/$arch"); include(payload) }
    // module.prop carries the version; the template lives outside assets so a bump is no diff.
    // It takes the release code, not versionCode: it is hashed into PAYLOAD_VERSION, and the
    // commit count would make every APK update redeploy binaries that did not change.
    from("src/main/boot-module/module.prop") {
        into("boot-module")
        expand(mapOf("version" to vapVersionName, "versionCode" to vapReleaseCode))
    }
    inputs.dir("src/main/assets/boot-module")
    inputs.property("version", vapVersionName)
    inputs.property("versionCode", vapReleaseCode)
    into(assetsDir)
    doFirst {
        // Builds before 2026-10-31 staged these two under src/main; a leftover tree
        // there would be merged alongside this one.
        file("src/main/assets/bin").deleteRecursively()
        file("src/main/assets/backend").deleteRecursively()
        for (arch in arches) for (bin in payload) {
            require(File(backendDir, "$arch/$bin").isFile) { "prepareAssets: backend/$arch/$bin is missing, run scripts/build-static.sh first" }
        }
    }
    doLast {
        for (arch in arches) {
            val dir = assetsDir.get().dir("bin/$arch").asFile
            val digest = MessageDigest.getInstance("SHA-256")
            for (bin in dir.listFiles()!!.sortedBy { it.name }) {
                digest.update(bin.name.toByteArray())
                digest.update(bin.readBytes())
            }
            val bootAssets = file("src/main/assets/boot-module").listFiles()!!.toList() +
                assetsDir.get().file("boot-module/module.prop").asFile
            for (asset in bootAssets.sortedBy { it.name }) {
                digest.update(asset.name.toByteArray())
                digest.update(asset.readBytes())
            }
            File(dir, "PAYLOAD_VERSION").writeText(digest.digest().joinToString("") { "%02x".format(it) })
        }
    }
}
tasks.named("preBuild") { dependsOn(prepareAssets) }

dependencies {
    // 2026.06.01 is the last BOM on the Compose 1.11 line; 1.12 wants compileSdk 37 and AGP 9.
    implementation(platform("androidx.compose:compose-bom:2026.06.01"))
    implementation("androidx.compose.ui:ui")
    // material3 1.4.0 stable stripped every Expressive API (LoadingIndicator, MaterialShapes).
    // 1.5.0-alpha18 is the last alpha built against Compose 1.11; alpha20 onward needs 1.12.
    implementation("androidx.compose.material3:material3:1.5.0-alpha18")
    implementation("androidx.compose.material:material-icons-extended")

    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.7.0")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.7.0")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.7.0")
    implementation("androidx.activity:activity-compose:1.8.2")
    // The in-app language on Android 8 to 12, which the framework only offers from 13
    implementation("androidx.appcompat:appcompat:1.7.0")

    // Root execution
    implementation("com.github.topjohnwu.libsu:core:5.2.1")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3")

    // QR encoding (Wi-Fi join code)
    implementation("com.google.zxing:core:3.5.3")
}
