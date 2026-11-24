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

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }

    buildTypes {
        release { isMinifyEnabled = false }
    }
}
