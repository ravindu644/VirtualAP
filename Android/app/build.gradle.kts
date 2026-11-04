import java.security.MessageDigest

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

// One version for the app and the backend it ships: VERSION at the repository root.
val vapVersionName = rootProject.file("../VERSION").readText().trim()
val vapVersionCode = vapVersionName.split(".").let { parts ->
    require(parts.size == 3 && parts.all { it.toIntOrNull() != null }) { "VERSION must be X.Y.Z, got '$vapVersionName'" }
    parts[0].toInt() * 10000 + parts[1].toInt() * 100 + parts[2].toInt()
}

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
// a hash of its binaries; the app compares it to its last deploy to know when an
// APK update ships new ones.
val backendDir = rootProject.file("../backend")
val arches = listOf("aarch64", "armhf")
// The payload, by name: backend/<arch> may still hold binaries an older build
// left there, which nothing runs any more.
val payload = listOf("virtualap", "hostapd", "dnsmasq")
val assetsDir = layout.buildDirectory.dir("generated/assets")
android.sourceSets["main"].assets.srcDir(assetsDir)

val prepareAssets = tasks.register<Sync>("prepareAssets") {
    for (arch in arches) from(File(backendDir, arch)) { into("bin/$arch"); include(payload) }
    into(assetsDir)
    doFirst {
        // Builds before 2026-10-31 staged these two under src/main; a leftover tree
        // there would be merged alongside this one.
        file("src/main/assets/bin").deleteRecursively()
        file("src/main/assets/backend").deleteRecursively()
        for (arch in arches) for (bin in payload) {
            // virtualap comes from its own build, the others from the container's.
            val how = if (bin == "virtualap") "make aarch64 armhf" else "scripts/build-static.sh"
            require(File(backendDir, "$arch/$bin").isFile) { "prepareAssets: backend/$arch/$bin is missing, run $how first" }
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
