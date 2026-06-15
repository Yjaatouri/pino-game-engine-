# Pino Game Engine — How It Works

A lightweight C++17 3D game engine with OpenGL ES 3.0 rendering, targeting Windows/macOS/Linux (via SDL2), Android (NativeActivity + EGL), and iOS (EAGL).

---

## Project Structure

```
pino-game-engine/
├── engine/              # Core engine library
│   ├── core/            # Math, logging, config, timers, events
│   ├── platform/        # Abstract + platform-specific (SDL2, Android, iOS)
│   ├── renderer/        # GLES3 rendering pipeline
│   ├── scene/           # Scene graph, entities, scene manager
│   ├── input/           # Input mapping, recording/replay
│   ├── physics/         # AABB collision world
│   └── assets/          # Asset loading, caching, packing
├── examples/            # Demos: triangle, cube, FPS, lit scene, games
├── cmake/               # Build options, dependency fetching
├── android/             # Android Gradle + CMake build
├── ios/                 # iOS Xcode project
├── CMakeLists.txt       # Root CMake build
└── README.md
```

---

## 1. Engine Core (`engine/core/`)

| File | Purpose |
|---|---|
| `types.h` | Portable typedefs: `i8`–`i64`, `u8`–`u64`, `f32`, `f64`, `usize` |
| `log.h` / `logger.h/cpp` | `Logger` singleton (thread-safe). Writes to stderr + file. Macros: `PINO_DEBUG/INFO/WARN/ERROR`. Stripped in release. |
| `config_loader.h/cpp` | Loads `config.ini` (window size, vsync, log level, etc.) or uses defaults. |
| `engine_stats.h` | Rolling 60-frame FPS, frame_time, update_time, render_time averages. |
| `timer.h` | `TimerManager`: `after(delay, cb)`, `every(interval, cb, times)`. Timers advance at fixed update rate. |
| `math_utils.h/cpp` | `Math` namespace: clamp, lerp, smoothstep, remap, radians/degrees, wrap. `Ray` struct, `rayAABBIntersection()`. `Random` class with deterministic seeding and unit-sphere sampling. |
| `transform.h/cpp` | `Transform`: position (vec3), rotation (quat), scale (vec3). Methods: `matrix()`, `forward/right/up()`, `look_at()`, static `lerp()`. |
| `event_bus.h` | `EventBus` singleton: type-safe publish/subscribe. Predefined: `CollisionEvent`, `InputEvent`, `SceneLoadedEvent`, `EntityDestroyedEvent`. Safe unsubscribe during dispatch. |

### Engine & Game Loop (`engine/engine.h/cpp`, `engine/game.h`)

```
main()
  ├── Engine engine
  ├── engine.init(cfg)           // Logger, config, Window, Input, FileSystem, GL pointers
  ├── engine.run(MyGame)         // implements IGame
  │     ├── game.init()          // setup shaders, meshes, scenes
  │     └── loop:
  │           ├── begin_frame()  // poll events, process input, calc dt
  │           ├── fixed_update   // game.update(FIXED_DT) at 60 Hz (default)
  │           ├── timers advance
  │           ├── game.render()  // variable-rate render
  │           └── end_frame()    // swap buffers
  └── engine.shutdown()
```

Key design: **fixed timestep update** with **variable render interpolation**. Physics/logic runs at a fixed rate regardless of display refresh.

---

## 2. Platform Abstraction (`engine/platform/`)

Interfaces: `Window`, `Input`, `FileSystem`.

Factory function `create_*()` picks the right implementation via `#ifdef`:

| Platform | Window | Input | FileSystem |
|---|---|---|---|
| **Desktop (SDL2)** | `Sdl2Window` — SDL2 + OpenGL ES/Core context | `Sdl2Input` — keyboard, mouse, gamepad (4), multi-touch, gestures | `Sdl2FileSystem` — `fstream` + `SDL_GetBasePath()` |
| **Android** | `AndroidWindow` — EGL + NativeActivity, context loss/restore | `AndroidInput` | `AndroidFileSystem` — AAssetManager |
| **iOS** | `IOSWindow` — EAGLView/EAGLContext | `IOSInput` | `IOSFileSystem` |

---

## 3. Renderer (`engine/renderer/`)

| Component | Purpose |
|---|---|
| `i_renderer.h` | Abstract `IRenderer` interface (shader, buffer, texture handles) |
| `gl_es3.h/cpp` | OpenGL ES 3.0 function pointer loading via `SDL_GL_GetProcAddress` / `eglGetProcAddress` / `dlsym` |
| `shader.h/cpp` | Compile vertex+fragment shaders into GL program. Uniform caching with dirty-check. |
| `mesh.h/cpp` | VAO/VBO/EBO management. Upload vertices+indices, draw, bounding box. Static `create_sphere()`. |
| `texture.h/cpp` | Upload RGBA textures, create checkerboard, cubemap from 6 faces. |
| `camera.h/cpp` | Perspective projection, look_at, orbit (yaw/pitch), view/projection/view_proj matrices. |
| `light.h/cpp` | `AmbientLight`, `DirectionalLight`, `PointLight`, `Material`. Upload functions. MAX 8 point lights. |
| `light_component.h` | `LightComponent` — type, color, attenuation, auto-sync from Entity transform. |
| `framebuffer.h/cpp` | FBO with color texture + optional depth renderbuffer. Move semantics. |
| `render_state.h/cpp` | Cached GL state with dirty-checking (depth, blend, cull, viewport). Push/pop stack (depth 8). Tracks state changes via `RenderStats`. |
| `render_stats.h/cpp` | Per-frame counters: draw_calls, triangles, shader_switches, state_changes. |
| `per_frame_ubo.h/cpp` | GL uniform buffer for view_proj, camera_pos, ambient, directional light. |
| `debug_renderer.h/cpp` | Line/box/sphere/axes drawing with built-in GLSL shaders. Batched per frame. |
| `shadow_map.h/cpp` | Depth-only FBO for directional light shadow mapping. Orthographic projection. 3x3 PCF in shader. |
| `skybox.h/cpp` | Cubemap shader + unit cube mesh + 6 face textures. Renders at end with `depth LEQUAL + no translation`. |
| `fps_controller.h/cpp` | WASD + mouse look attached to Camera. Configurable speed/sensitivity. |
| `metal_renderer.h/.mm` | Stub `IRenderer` for future Metal migration. Not functional. |

### Per-Frame Rendering Pipeline

```
1. Clear color + depth buffers
2. Shadow pass: render depth from light's POV
3. Bind lit shader
4. Upload uniforms: view-proj, model matrices, lights, material
5. Bind textures (diffuse, shadow map)
6. For each visible Entity: draw Mesh
7. DebugRenderer: lines/boxes/spheres
8. SkyboxRenderer: depth LEQUAL, no camera translation
```

---

## 4. Scene Graph (`engine/scene/`)

| Component | Purpose |
|---|---|
| `i_scene.h` | `IScene` interface: init, update, render, shutdown + lifecycle hooks (on_enter/on_exit/on_pause/on_resume) |
| `scene.h/cpp` | Root Entity, `find_by_name()`, `find_all_with_tag()`, `raycast()` (AABB), `for_each()` / `for_each_active()` |
| `scene_manager.h` | Stack-based scene management with deferred push/pop/replace. `flush()` applies at safe frame boundary. |
| `entity.h/cpp` | Entity: name, active, tags, Transform, parent/child hierarchy (unique_ptr). World matrix/position, local AABB, destroy callback. Safe recursive traversal (snapshots children). |

---

## 5. Input System (`engine/input/`)

| Component | Purpose |
|---|---|
| `input_map.h` | String action → key/gamepad-button binding with context stack (Gameplay/UI/Debug). Query: `is_action_pressed/just_pressed/released()`. |
| `input_recorder.h` | Binary recording/playback of full InputState per frame. Magic "PIR1" header. Used for deterministic regression testing. |
| `gamepad.h/cpp` | Manages up to 4 SDL game controllers. Normalized axes with deadzone, rumble, prev-state tracking. |

---

## 6. Physics (`engine/physics/`)

| Component | Purpose |
|---|---|
| `aabb.h` | `AABB` struct: min/max, `from_center_extents()`, `from_transform()`, `overlaps()`, `push_out()` (MTV on penetration axis), `contains()`. |
| `collider_component.h` | `ColliderComponent`: local min/max, world_aabb, is_static, enabled, collision_layer/mask bitfields. `update_world_aabb()`, `rayAABBIntersection()` with normal. |
| `collision_world.h/cpp` | Register/unregister colliders per-entity. `update()` refreshes world AABBs, detects overlaps, emits `CollisionEvent` via EventBus, resolves via push-out, raycasts, overlap_aabb queries. Auto-unregisters on `EntityDestroyedEvent`. |
| `debug_draw.h/cpp` | Wireframe AABB rendering with built-in GLSL shader. |

Collision filtering: two entities collide only when each one's `collision_layer` matches the other's `collision_mask`.

---

## 7. Assets (`engine/assets/`)

| Component | Purpose |
|---|---|
| `asset_manager.h/cpp` | `load_mesh()` (tinyobjloader), `load_texture()` (stb_image), `load_shader()`. Caches by normalized path. `AssetHandle<T>` shared_ptr. Fallback assets: checker texture, unit cube, simple shader. `preload()`, `unload_unused()` (refcount=1), `invalidate_all()` (context loss). |
| `asset_pack.h/cpp` | `AssetPackReader`: binary pack format (magic "PINO", FNV-1a 64-bit hashed names). `write_asset_pack()` offline packer. |
| `stb_image.cpp` | Single translation unit with `STB_IMAGE_IMPLEMENTATION`. |

---

## 8. External Dependencies

| Library | Version | Use |
|---|---|---|
| **SDL2** | 2.26.5 | Desktop window, GL context, input, file base path |
| **GLM** | 0.9.9.8 | All math: vec3, mat4, quaternions, transforms, perspective |
| **tinyobjloader** | v2.0.0rc10 | OBJ mesh loading |
| **stb_image** | 2024-05-31 | Texture image loading (PNG, JPG, BMP, TGA) |

All fetched via CMake `FetchContent` (or system SDL2 via `PINO_USE_SYSTEM_SDL2`).

---

## 9. Notable Patterns

- **Platform abstraction** via virtual interfaces + factory functions with `#ifdef`
- **Singletons**: Logger, EventBus, RenderState, RenderStats, Input
- **Fixed timestep** game loop with accumulator pattern
- **Dirty-check rendering**: only issue GL call when state actually changes
- **Copy-list dispatch**: EventBus copies subscriber list before emitting, allowing safe unsubscribe during dispatch
- **Safe entity traversal**: child pointer snapshot before iteration
- **Deferred scene ops**: SceneManager queues push/pop/replace, applies at frame boundary
- **Move semantics**: Mesh, Texture, Framebuffer, ShadowMap use move ctor/assignment
- **Asset fallbacks**: never return null — checker texture / unit cube / simple shader
- **Context loss**: `IGame::on_context_lost/restored()` + `AssetManager::invalidate_all()`
- **Input recording**: full InputState serialization to binary for deterministic replay
- **Collision layers**: bitmask-based filtering per collider pair

---

## 10. Examples

| Example | What it demonstrates |
|---|---|
| `clear/` | Minimal: clear screen to color |
| `triangle/` | Rotating colored triangle (inline GLSL) |
| `cube/` | Textured rotating cube, orbiting camera |
| `fps_camera/` | First-person WASD + mouse look |
| `scene_graph/` | Entity parent/child hierarchy |
| `asset_demo/` | OBJ + texture via AssetManager |
| `lit_scene/` | Full Phong lighting: ambient + directional + point |
| `collision_demo/` | AABB collision detection + resolution |
| `input_test/` | Interactive keyboard/mouse/touch debug |
| `game_demo/` | Top-down arena game: enemies, coins, lives, particles, camera shake, scenes |
| `demo_game/` | Polished arena game: title screen, enemy scaling, score, game over |
| `stability_test/` | Cycles init/run/shutdown to test for leaks/crashes |
| `arena_game/` | Another arena game variant |
| `ios_lit_cube/` | iOS app rendering a lit cube via Pino |
| `android_lit_cube/` | Android NativeActivity rendering a lit cube |
