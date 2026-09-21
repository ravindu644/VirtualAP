import java.util.Properties
import java.io.FileInputStream
import java.security.MessageDigest

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val keystorePropertiesFile = rootProject.file("local.properties")
val keystoreProperties = Properties()
if (keystorePropertiesFile.exists()) {
    FileInputStream(keystorePropertiesFile).use { keystoreProperties.load(it) }
}

// Read version from the VERSION file at the repository root
fun getVAPVersion(): String {
    val versionFile = file("../../VERSION")
    if (!versionFile.exists()) return "1.0.0"
    return versionFile.readText().trim().ifEmpty { "1.0.0" }
}

val vapVersionName = getVAPVersion()
val vapVersionCodeVal = vapVersionName.split(".").let { parts ->
    try {
        val major = parts.getOrNull(0)?.toInt() ?: 1
        val minor = parts.getOrNull(1)?.toInt() ?: 0
        val patch = parts.getOrNull(2)?.toInt() ?: 0
        major * 1000 + minor * 100 + patch * 10
    } catch (e: Exception) {
        1
    }
}

android {
    namespace = "com.virtualap.app"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.virtualap.app"
        minSdk = 26
        targetSdk = 34
        versionCode = vapVersionCodeVal
        versionName = vapVersionName

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        vectorDrawables {
            useSupportLibrary = true
        }
        // No native libs are bundled in the APK - the AP-stack binaries ship as
        // assets (assets/bin/<arch>/) and are deployed at runtime for the device's
        // arch. So no abiFilters: the same APK installs on both arm64-v8a and
        // armeabi-v7a phones.
    }

    signingConfigs {
        val keystoreFile = file("../../virtualap.keystore")
        val keystorePassword = keystoreProperties["KEYSTORE_PASSWORD"] as String?
            ?: project.findProperty("KEYSTORE_PASSWORD") as String?
            ?: ""
        val keyAliasName = keystoreProperties["KEY_ALIAS"] as String?
            ?: project.findProperty("KEY_ALIAS") as String?
            ?: "virtualap"
        val actualKeyPassword = keystoreProperties["KEY_PASSWORD"] as String?
            ?: project.findProperty("KEY_PASSWORD") as String?
            ?: keystorePassword

        if (keystoreFile.exists() && keystorePassword.isNotEmpty()) {
            getByName("debug") {
                storeFile = keystoreFile
                storePassword = keystorePassword
                keyAlias = keyAliasName
                keyPassword = actualKeyPassword
                enableV2Signing = true
                enableV3Signing = true
                enableV4Signing = true
            }
            create("release") {
                storeFile = keystoreFile
                storePassword = keystorePassword
                keyAlias = keyAliasName
                keyPassword = actualKeyPassword
                enableV2Signing = true
                enableV3Signing = true
                enableV4Signing = true
            }
        } else {
            var fallbackKeystore = file(System.getProperty("user.home") + "/.android/debug.keystore")

            if (!keystoreFile.exists() && !fallbackKeystore.exists()) {
                println("WARNING: No keystore found. Generating temporary CI keystore...")
                val ciKeystore = file("../../ci-debug.keystore")
                if (!ciKeystore.exists()) {
                    project.exec {
                        commandLine(
                            "keytool", "-genkeypair",
                            "-v",
                            "-keystore", ciKeystore.absolutePath,
                            "-alias", "virtualap",
                            "-keyalg", "RSA",
                            "-keysize", "2048",
                            "-validity", "10000",
                            "-storetype", "PKCS12",
                            "-storepass", "android",
                            "-keypass", "android",
                            "-dname", "CN=VirtualAP, OU=Mobile, O=Anonymized, L=Earth, ST=Galaxy, C=UN",
                            "-sigalg", "SHA256withRSA"
                        )
                    }
                }
                fallbackKeystore = ciKeystore
            }

            println("Using fallback keystore: ${fallbackKeystore.absolutePath}")
            getByName("debug") {
                storeFile = fallbackKeystore
                storePassword = "android"
                keyAlias = if (fallbackKeystore.name.contains("ci-debug")) "virtualap" else "androiddebugkey"
                keyPassword = "android"
            }
            create("release") {
                initWith(getByName("debug"))
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            isDebuggable = false
            signingConfig = signingConfigs.getByName("release")
        }
        debug {
            isMinifyEnabled = false
            isDebuggable = true
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
        freeCompilerArgs += listOf(
            "-opt-in=kotlin.RequiresOptIn",
            "-Xjvm-default=all"
        )
    }

    buildFeatures {
        compose = true
    }

    composeOptions {
        kotlinCompilerExtensionVersion = "1.5.8"
    }

    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }
}

// prepareAssets: stages the backend script and the static AP-stack binaries
// into the assets directory. Binaries are staged per-arch under
// assets/bin/<arch>/ (aarch64, armhf), each with its own PAYLOAD_VERSION marker
// (a hash of that arch's binaries) so the app can detect when an APK update
// ships new binaries. The app deploys only the arch matching the device.
tasks.register("prepareAssets") {
    // Declared so gradle can skip the copy when nothing under backend/ changed.
    inputs.dir(file("../../backend"))
    outputs.dir(file("src/main/assets/bin"))
    outputs.file(file("src/main/assets/backend/start-ap"))
    doLast {
        val toolsSrc = file("../../backend")
        val assetTools = file("src/main/assets/backend")
        val assetBin = file("src/main/assets/bin")

        // Drop any stale chroot-era assets (rootfs tarball, vap.sh).
        file("src/main/assets/rootfs").deleteRecursively()
        assetTools.mkdirs()
        assetBin.deleteRecursively(); assetBin.mkdirs()
        File(assetTools, "vap.sh").delete()

        // Copy start-ap
        val startAp = File(toolsSrc, "start-ap")
        require(startAp.exists()) { "prepareAssets: start-ap not found at ${startAp.absolutePath}" }
        startAp.copyTo(File(assetTools, "start-ap"), overwrite = true)
        println("prepareAssets: copied start-ap")

        // Copy the static binaries (busybox + hostapd/hostapd_cli/iw/dnsmasq)
        // for each supported arch. Skip dotfiles like .gitkeep - only real
        // binaries ship.
        val arches = listOf("aarch64", "armhf")
        var staged = 0
        for (arch in arches) {
            val binSrc = File(toolsSrc, arch)
            val binaries = binSrc.listFiles()
                ?.filter { it.isFile && !it.name.startsWith(".") }
                ?.sortedBy { it.name } ?: emptyList()
            if (binaries.isEmpty()) {
                println("prepareAssets: WARNING no binaries in ${binSrc.absolutePath} - skipping $arch")
                continue
            }
            val archDir = File(assetBin, arch).apply { mkdirs() }
            val digest = MessageDigest.getInstance("SHA-256")
            for (bin in binaries) {
                bin.copyTo(File(archDir, bin.name), overwrite = true)
                digest.update(bin.name.toByteArray())
                digest.update(bin.readBytes())
                println("prepareAssets: copied $arch/${bin.name}")
            }
            // Marker = hash of every binary, so it changes iff the binaries change.
            val marker = digest.digest().joinToString("") { "%02x".format(it) }
            File(archDir, "PAYLOAD_VERSION").writeText(marker)
            println("prepareAssets: $arch payload version $marker")
            staged++
        }
        require(staged > 0) {
            "prepareAssets: no binaries staged for any arch under ${toolsSrc.absolutePath}. " +
                "Run scripts/build-static.sh first."
        }
    }
}

tasks.configureEach {
    if (name.startsWith("pre") && name.endsWith("Build")) {
        dependsOn("prepareAssets")
    }
}

dependencies {
    // Compose BOM
    implementation(platform("androidx.compose:compose-bom:2024.06.00"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-graphics")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended")
    // Required by ui/util/LoadingIndicator.kt (Material 3 Expressive shapes)
    implementation("androidx.graphics:graphics-shapes:1.0.1")

    // Core Android
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.core:core-splashscreen:1.0.1")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.7.0")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.7.0")
    implementation("androidx.activity:activity-compose:1.8.2")

    // Navigation
    implementation("androidx.navigation:navigation-compose:2.7.7")

    // ViewModel
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.7.0")

    // Root execution - libsu
    implementation("com.github.topjohnwu.libsu:core:5.2.1")
    implementation("com.github.topjohnwu.libsu:service:5.2.1")
    implementation("com.github.topjohnwu.libsu:io:5.2.1")

    // Coroutines
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.7.3")

    // QR encoding (Wi-Fi join code)
    implementation("com.google.zxing:core:3.5.3")

    // Debug
    debugImplementation("androidx.compose.ui:ui-tooling")
    debugImplementation("androidx.compose.ui:ui-test-manifest")
}
