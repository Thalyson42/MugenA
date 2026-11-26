plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "io.github.thalyson42.mugen"
    compileSdk = 35
    ndkVersion = "27.2.12479018"

    defaultConfig {
        applicationId = "io.github.thalyson42.mugen"
        minSdk = 24
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0-p1"

        // D-001: arm64-v8a only, native execution.
        ndk { abiFilters += "arm64-v8a" }
    }

    externalNativeBuild {
        cmake {
            // The repository's top-level CMakeLists.txt; its ANDROID branch is cmake/MugenAndroid.cmake.
            path = file("../../CMakeLists.txt")
            version = "3.22.1"
        }
    }

    sourceSets {
        // P3b (D-015): the Java side of SDL2 (SDLActivity & co.), vendored unmodified except the marked HID line.
        getByName("main").java.srcDir("../../third_party/SDL2/android-project/app/src/main/java")
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }

    // D-016: a fixed DEBUG key kept in the repo, so that every CI build is signed by the same certificate and Android
    // updates the installed app in place (a new key per run forced an uninstall, which also wipes Android/data). It is a
    // debug-only key (public password "android"): never used for release; the release key is P12 (GitHub Secrets).
    signingConfigs {
        getByName("debug") {
            storeFile = file("../debug.keystore")
            storePassword = "android"
            keyAlias = "androiddebugkey"
            keyPassword = "android"
        }
    }

    buildTypes {
        release { isMinifyEnabled = false }
    }
}
