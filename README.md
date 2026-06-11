# Pino Game Engine

Lightweight C++17 3D game engine with OpenGL ES 3.0 rendering.

## Platforms

- **Windows / macOS / Linux** — SDL2 desktop build
- **Android** — NativeActivity + EGL build (API 26+)
- **iOS** — EGL/EAGL build (14+)

## Desktop Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/bin/Debug/arena_game.exe
```

## Android Build (CI / command line)

No Android Studio required.

### Prerequisites

- Android NDK (r25+ recommended)
- Gradle 8.4+ (wrapper included in `android/`)
- CMake 3.22+

### NDK Path Configuration

Set the `ANDROID_NDK_HOME` environment variable:

```bash
export ANDROID_NDK_HOME=/path/to/android-ndk-r25c
```

Or create `android/local.properties`:

```
ndk.dir=/path/to/android-ndk-r25c
sdk.dir=/path/to/android-sdk
```

### Build Commands

**Debug build** (logging enabled, no optimisations):

```bash
cd android
./gradlew assembleDebug
```

APK output: `android/app/build/outputs/apk/debug/app-debug.apk`

**Release build** (logging stripped, optimised):

```bash
cd android
./gradlew assembleRelease
```

APK output: `android/app/build/outputs/apk/release/app-release.apk`

**Install to device**:

```bash
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
```

### Gradle Wrapper

If `gradlew` is missing, generate it:

```bash
cd android
gradle wrapper --gradle-version 8.4
```

### Build Variants

| Variant  | Logging | Optimisation | GL Debug |
|----------|---------|-------------|----------|
| `debug`  | Enabled | `-O0 -g`    | Yes      |
| `release`| Stripped| `-O2`       | No       |

## Architecture

```
engine/
├── core/          # Math, transforms, logging, config, timers
├── platform/      # Window, Input, FileSystem (SDL2 / Android / iOS)
├── renderer/      # GL loader, shaders, meshes, textures, lights, shadowmap, skybox
├── assets/        # AssetManager, asset packer, stb_image
├── scene/         # Entity/Scene graph, EventBus, SceneManager
├── input/         # InputMap, GamepadManager, InputRecorder
└── physics/       # AABB collision, CollisionWorld
```

## License

MIT
