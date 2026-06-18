# Pino Game Engine — Official Development Roadmap

**Current version:** v0.1.0 — Engine Core Foundation Stable

This document is the official development roadmap for the Pino Game Engine, established after architectural review at v0.1.0. It defines the sequence, rationale, and dependencies for all major systems leading to an indie-game-ready engine. The roadmap is a plan, not a contract — priorities may shift based on real-world development feedback, but the dependency ordering and architectural reasoning documented here should be respected to avoid rework.

---

## Stage 1 — Audio Integration

**Library:** miniaudio (single-header C library, `ma_engine` high-level API)

Add basic audio playback: load sound files, one-shot SFX, controllable looping sources (music, ambient), volume control per-source and master. No 3D audio, no DSP, no streaming — just minimal useful audio.

*Zero dependencies. Fast win that makes every demo feel more complete.*

---

## Stage 2 — Diagnostics & Memory Tracking

Profiling macros (scoped timers, frame stats), assertion macros, debug draw (wireframe primitives via a new line-drawing shader path), allocation tracking macros for mobile memory pressure awareness, and a minimal bitmap text helper for rendering FPS/counter overlays. The bitmap text helper is intentionally simple — fixed-size glyph atlas, textured quads, no layout engine. A full text rendering system is deferred until a custom game UI is designed.

*Without this stage, every subsequent system is developed without visibility into its own performance or correctness.*

---

## Stage 3 — Serialization Foundation

Define the shared binary format primitive used by all future engine systems: chunk header with type tag and version, endian handling, string table, type registry for forwards/backwards compatibility. This is not scene serialization yet — it is the lowest-level encoding layer that both the asset pipeline (stage 4) and scene/save serialization (stage 7) will share.

*Critically prevents format fragmentation — one binary encoding scheme for all engine data, not two unrelated formats for assets versus game state.*

---

## Stage 4 — Asset Pipeline

Cooked asset format built on the serialization foundation. Mesh optimization (vertex cache reordering, tangent/bitangent generation for normal mapping), texture compression with mipmap generation, material definitions referencing cooked textures and shader parameters, and a runtime asset manifest loaded in a single I/O pass at startup. Everything that follows (physics colliders, animation skinned data, particle textures, editor content) loads through this pipeline.

*Highest-leverage infrastructure investment. Without it, every system loads raw files ad-hoc, creating inconsistency and fragility.*

---

## Stage 5 — Physics Expansion

Extend from basic AABB collision to raycasting (picking, shooting, AI line-of-sight), trigger volumes (zones, damage areas), sphere and capsule queries, and a basic character controller with swept collision. Built on the existing scene graph and transform system. No dependency on serialization or the asset pipeline — physics collider data can be authored in code or loaded through the pipeline when ready.

*Directly enables gameplay. No prototype-level game can ship without raycasting and a character controller.*

---

## Stage 6 — Renderer Expansion

Explicitly scoped to what is required for animation, particles, and editor visualization. Shader compilation, caching, and permutation management. Material system with texture slot assignment, uniform binding, and render state configuration. GPU instancing for static geometry. Basic draw-call sorting by material to maximize batching. Line and point primitive shader for debug draw. All designed with mobile GPU constraints (uniform limits, instance count limits, tile-based renderer considerations) baked in from the start.

*The largest single stage by effort. Must be explicitly bounded to avoid scope creep into full PBR or deferred shading, which are not on this roadmap.*

---

## Stage 7 — Serialization Applications

Scene serialization (save/load the complete entity hierarchy with component state), save data format for game progress, and engine configuration system (runtime settings file replacing the hardcoded EngineConfig struct). Reuses the serialization foundation from stage 3 and the asset manifest convention from stage 4.

*Enables editor checkpointing, game save systems, and user-configurable engine settings.*

---

## Stage 8 — Skeletal Animation

Bone hierarchy with linear blend skinning on the GPU, animation clips loaded from the cooked asset pipeline, clip playback with blending and crossfade. Character models with skinned meshes rendered through the expanded renderer. No blend trees, inverse kinematics, or retargeting in the initial pass — minimum viable animation for characters and creatures.

---

## Stage 9 — Particle System

GPU-instanced particle rendering with billboard quads, emitter configuration (rate, lifetime, speed, spread, color over lifetime), and spritesheet animation support. Particle texture data loaded through the asset pipeline. Lightweight warm-up before tackling scripting integration.

---

## Stage 10 — ImGui-Based Editor MVP

Dear ImGui integrated as a development-only dependency (not shipped in runtime builds). Scene hierarchy tree view, property inspector driven by serialization metadata from stage 7, console output window, asset browser browsing the pipeline manifest, debug visualization toggles, and viewport with transform gizmos using renderer and debug draw infrastructure from stages 2 and 6. ImGui is used for the editor and development tools only — runtime game UI is a separate future system.

---

## Stage 11 — Scripting Integration

Lightweight scripting VM (Lua or similar) exposed through a clean C-ABI binding layer. Engine API surface validated during stage 7 serialization work with a minimal binding prototype. Game logic (spawning entities, playing sounds, querying input, starting cutscenes) callable from scripts without engine recompilation. Not for performance-critical inner loops — those remain in C++.

---

## Stage 12 — Packaging & Deployment

Automated packaging step in the build system: collect executable, cooked assets from the pipeline manifest, and runtime dependencies into a distributable archive with a version manifest. Platform-specific installer or archive format. Documentation bundling.

---

## Stage 13 — Networking (Deferred)

UDP-based remote procedure call layer on top of the serialization format from stage 3/7, treating remote peers as synchronized scene patches. Not on this roadmap for active development — deferred until the engine has shipped at least one single-player title.

---

## Architectural Invariants

- **ImGui is editor-only.** Runtime game UI is a future custom system, not the same code path as the editor.
- **One binary format.** All engine data (cooked assets, scenes, save files, configuration) uses the shared serialization foundation from stage 3. No format fragmentation.
- **Mobile-first rendering.** The renderer expansion in stage 6 is designed with mobile GPU constraints as requirements, not afterthoughts.
- **Documentation is incremental.** Every stage from stage 4 onward produces documentation for the public API it introduces. No separate documentation stage.
- **Text rendering is deferred.** A full text rendering system belongs with the custom game UI, not before it. The debug overlay in stage 2 uses a minimal bitmap character blitter with no layout engine.
- **No PBR, no deferred shading.** The renderer remains forward rendering with the existing GL ES 3.0 feature set. Scope creep into full physically-based rendering or deferred pipelines is not on this roadmap.
