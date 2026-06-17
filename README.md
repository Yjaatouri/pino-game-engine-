# Pino Game Engine

## Pino Game Engine v0.1.0

Engine Core Foundation Stable.

- **Stable engine foundation** — cleaned engine initialization, deterministic platform branching, removed dead code paths.
- **Deterministic asset system** — runtime asset root discovery via `find_asset_root()`; no fragile CMake path dependency.
- **Safe GL loader** — critical OpenGL ES 3.0 function validation; `gl::init()` returns `false` on missing entry points.
- **Unified logging system** — project-prefixed `PINO_*` macros only; legacy `LOG_*` macros removed.
- **Clean SDL integration** — SDL headers fully decoupled from project headers (`sdl2_input.h`, `sdl2_window.h`, `gamepad.h` use portable types only).
- **Simplified EngineConfig system** — platform-safe data flow; fixed latent iOS compile error; no redundant field assignments.
- **Lean CMake configuration** — removed 4 dead user-facing options; 3 live options remain.

This release focuses on engine stability and internal architecture cleanup. It is not a feature-complete engine, but a stable base for future systems such as Audio, UI, and advanced rendering.

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
