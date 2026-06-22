# Pino Game Engine

Lightweight C++17 3D game engine with OpenGL ES 3.0 rendering, targeting Windows/macOS/Linux (SDL2), Android (NativeActivity + EGL), and iOS (EAGL).

## Status

Engine core foundation is stable. ECS system, renderer, and platform layer are functional. Higher-level systems (audio, physics, animation, editor, scripting) are in various stages of completion — see [ARCHITECTURE_AUDIT.md](ARCHITECTURE_AUDIT.md) for full details.

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
├── core/          # Types, math, transforms, logging, config, timers, event bus, engine context
├── platform/      # Abstract Window/Input/FileSystem + SDL2 / Android / iOS backends
├── renderer/      # GL ES 3.0 loader, shaders, meshes, textures, cameras, lights,
│                  # materials, shadow map, skybox, framebuffers, debug renderer,
│                  # text renderer, render queue, frustum, LOD, FPS controller
├── scene/         # Entity hierarchy (old tree-based), scene manager, IScene
├── input/         # InputMap (action bindings), InputRecorder, GamepadManager
├── physics/       # AABB collision, broad-phase (grid/SAP/brute-force), debug draw
├── audio/         # AudioManager (miniaudio): spatial audio, zones, buses, priorities
├── assets/        # AssetManager, asset pack, stb_image
├── ecs/           # Entity registry, component pools, scene graph, EcsWorld,
│                  # prefabs, ECS inspector, physics adapter
└── debug/         # Debug console, profiler overlay
```

## Dependencies

| Library | Version | Use |
|---------|---------|-----|
| **SDL2** | 2.26.5 | Desktop window, GL context, input, file base path |
| **GLM** | 0.9.9.8 | All math: vectors, matrices, quaternions, transforms |
| **tinyobjloader** | v2.0.0rc10 | OBJ mesh loading |
| **stb_image** | 2024-05-31 | Texture image loading (PNG, JPG, BMP, TGA) |
| **miniaudio** | 0.11.25 | Audio playback (desktop only) |

All fetched via CMake `FetchContent` (SDL2 can use system install via `PINO_USE_SYSTEM_SDL2`).

## License

MIT
