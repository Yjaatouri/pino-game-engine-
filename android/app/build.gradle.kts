plugins {
    id("com.android.application")
}

android {
    namespace = "com.pino.engine"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.pino.engine"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments("-DANDROID_STL=c++_shared")
                arguments("-DANDROID_PLATFORM=android-26")
                cppFlags("-std=c++17")
            }
        }
    }

    buildTypes {
        debug {
            isDebuggable = true
            externalNativeBuild {
                cmake {
                    arguments("-DPINO_BUILD_TYPE=debug")
                    cppFlags("-g -O0 -D_DEBUG")
                }
            }
        }
        release {
            isMinifyEnabled = false
            isDebuggable = false
            externalNativeBuild {
                cmake {
                    arguments("-DPINO_BUILD_TYPE=release")
                    cppFlags("-O2 -DNDEBUG")
                }
            }
        }
    }

    externalNativeBuild {
        cmake {
            path("CMakeLists.txt")
            version("3.22.1")
        }
    }

    sourceSets {
        getByName("main") {
            assets.srcDirs("src/main/assets")
        }
    }
}
