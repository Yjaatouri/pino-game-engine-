# Micro Arena Survival — Stress Test Report

## Overview

A top-down arena survival game exercising all major engine systems simultaneously:
**ECS**, **physics (CollisionWorld)**, **audio**, **asset pipeline**, **serialization**,
**profiler**, **text/HUD**, and **input**. Wave-based enemy spawning creates progressive
entity churn (3→50+ enemies), continuous physics simulation, and rapid audio playback.

---

## Issue 1 — One-Way Physics Resolution (Severity: HIGH)

**Observation:** `EcsScene::update_physics()` syncs transforms from ECS SceneGraph TO
CollisionWorld proxies, then runs collision detection and resolution. However, the
resolution (push-out) modifies the *proxy* Entity's transform, which is NEVER
synced BACK to the ECS SceneGraph. Result: physics wall collisions are a silent no-op
for ECS-managed entities.

**Root Cause:** `EcsPhysicsAdapter::sync()` is strictly directional: ECS → proxy.
`CollisionWorld::resolve_collisions()` pushes the proxy, but no reverse-sync mechanism
exists. The adapter's `sync_proxy_transform` is called only during the forward sync.

**Impact:** Any game using `EcsScene` + `EcsPhysicsAdapter` for entity physics cannot
rely on CollisionWorld for collision response. Entities will pass through walls.
Workaround (as in this stress test): manual arena bounds clamping.

**Reproduction:**
```cpp
scene.update_physics(cw, dt);  // syncs ECS→proxy, resolves proxy→proxy
glm::vec3 pos = sg.world_position(entity);  // ← unchanged by physics!
```

**Fix required:** Add a `sync_back` pass in `EcsScene::update_physics()` that reads
proxy world positions and writes them back to SceneGraph transforms.

---

## Issue 2 — No CollisionWorld ↔ ECS EntityId Back-Mapping (Severity: HIGH)

**Observation:** All CollisionWorld queries (`raycast`, `overlap_aabb`, collision
events) return tree-based `Entity*` objects. When using `EcsPhysicsAdapter`, these
are internal proxy entities named `"phys_proxy"` with no connection to the originating
ECS `EntityId`. Hitscan weapons, overlap queries, and collision callbacks are
effectively unusable with ECS entities.

**Root Cause:** The `EcsPhysicsAdapter` stores proxy → ECS mappings in a private
`m_proxies` map (keyed by `entity.index`). No public accessor or reverse-lookup is
exposed.

**Impact:** Hitscan weapons (`cw.raycast()`) cannot identify which ECS entity was hit.
Area-overlap queries (`cw.overlap_aabb()`) return proxy entities with no way to map
back to game objects. Collision events (`CollisionEnterEvent`) are similarly opaque.

**Workaround in this test:** Replaced raycast with distance-based area attack, which
iterates the enemy list and checks proximity directly — bypassing CollisionWorld
entirely for gameplay combat.

**Fix required:** Expose a `EntityId collider_to_entity(Entity*)` method on
`EcsPhysicsAdapter`, or add user-data storage on tree `Entity` (e.g., a `void*`
or `uint64_t` field) that the adapter can set during proxy creation.

---

## Issue 3 — EcsScene::destroy_entity() Inefficiency Under Churn (Severity: MEDIUM)

**Observation:** `EcsScene::destroy_entity()` performs 5 sequential operations per
entity: remove render component, remove physics component, remove audio component,
detach from scene graph, destroy in registry. Each `ComponentPool::remove()` does a
swap-with-last + free-list push. Under high churn (50+ enemies dying and respawning),
this creates O(n) component moves and cascading proxy cleanup on the next sync.

**Root Cause:** No batch-destroy API. No "deferred destroy" queue that can amortize
cleanup across frames. The EcsPhysicsAdapter's proxy cleanup (in `sync()`) iterates
ALL proxies each frame, not just dirtied ones.

**Impact:** During peak waves with rapid enemy kills (5+ deaths/second), the
destroy+spawn cycle stresses:
1. Component pool swap-remove (render, physics, audio — 3 pool operations)
2. SceneGraph detach (orphans children, clears node, updates free list)
3. Registry destroy (marks slot as free, bumps generation)
4. Next frame's physics sync (iterates all proxies to find stale ones)

**Reproduction:** Spawn 50 enemies, destroy all within 1 second while spawning 50
more. Profile `destroy_entity` and the subsequent `sync()`.

**Suggested future improvement:** Add `EcsScene::destroy_batch(span<EntityId>)` with
bulk cleanup, or a deferred-destroy queue.

---

## Issue 4 — Serialization At Exit Can Stall (Severity: MEDIUM)

**Observation:** The save-on-exit path serializes every entity in the ECS registry
(transform, render, physics, audio components) using `SaveGameSerializer`. With
50+ entities, each entity writes ~80–120 bytes across multiple `beginChunk`/`endChunk`
pairs and StringTable interactions. No progress reporting or timeout exists.

**Root Cause:** `SaveGameSerializer::serialize()` iterates all entities sequentially
using `registry().each(...)`. Each entity calls `writeEntity()` which opens a chunk,
writes all fields, and closes the chunk. StringTable deduplication also adds overhead.

**Impact:** On quit, the game freezes for a perceptible duration (potentially
100+ ms with 50 entities). In mobile/web contexts this could trigger an OS kill.

**Fix suggestion:** Add a `BinaryChunkWriter::reserve()` or streaming write path,
and optionally defer serialization to a background thread.

---

## Issue 5 — Uniform Upload Overhead With Many Entities (Severity: LOW-MEDIUM)

**Observation:** Each frame uploads per-entity PhongMaterial uniforms using
`upload_material()` followed by `cube_mesh->draw()`. This requires 5+ `glUniform*`
calls per entity (ambient, diffuse, specular, shininess, has_diffuse_tex, model,
normal_matrix). For 50 enemies + player + walls + floor ≈ 60 draw calls × 7 uniforms
≈ 420 glUniform calls per frame.

**Root Cause:** The engine uses a forward rendering pipeline with per-entity shader
uniforms. No instancing (beyond the dedicated instancing API in `Mesh::set_instance_data`)
or uniform buffer objects are used for entity materials.

**Impact:** At 60 FPS, uniform upload bandwidth is significant. Material changes per
entity prevent batching.

**Note:** This is an expected trade-off of forward rendering and not a bug. The
existing `RenderQueue` + instancing path can mitigate this, but was not used in this
test for simplicity.

---

## Architectural Observations

### 1. Dual entity system (tree Entity + ECS EntityId)
The engine has two parallel entity systems:
- **Tree-based** (`engine/scene/`): `Entity` with owned children, `Scene`
- **ECS-based** (`engine/ecs/`): `EntityId` handle, `EntityRegistry`, `SceneGraph`

They share no common base. CollisionWorld, DebugDraw, and Prefab systems use tree
entities. EcsScene, Serialization, and component pools use ECS handles. The
`EcsPhysicsAdapter` bridges them but with limitations (Issues 1, 2).

### 2. EcsScene has no built-in game-loop integration
`EcsScene` is a container of components + scene graph. It does NOT integrate with
`Engine::run(IGame)` — the user must write their own loop. Contrast with the
tree-based Scene which is used directly in arena_game, collision_demo, etc.

### 3. RenderQueue does not integrate with EcsScene
`EcsScene::update_render()` can submit commands to a RenderQueue, but the camera,
lights, and flush logic remain manual. No `<EcsScene, Camera, RenderQueue>` combined
helper exists. This is by design (flexibility) but means every game reimplements the
same setup.

### 4. Audio spatial system depends on tree Entity listener
`AudioManager::set_active_camera(Camera* cam)` only sets the listener. For ECS-based
games, there's no `update_audio` counterpart that auto-syncs an ECS audio entity's
transform to the listener position — `EcsScene::update_audio()` syncs emitter
positions but NOT the listener.

---

## Stress Test Performance Profile (Estimated)

| Metric | Value | Notes |
|--------|-------|-------|
| Max entities (ECS) | 55 | Player + 50 enemies + 4 walls (tree-only) |
| Max colliders (CW) | 57 | 55 game + floor + 1 player proxy + 50 enemy proxies + 4 wall + 1 floor |
| Draw calls/frame | ~60 | ~6 floor/walls + 1 player + 50 enemies + 2 text (HUD) |
| glUniform calls/frame | ~420 | 60 draw calls × 7 uniform sets |
| Audio one-shots/min | ~400 | 2/sec shooting + 1/sec kills at peak |
| ECS create-destroy/cycle | ~10 | Wave spawns + player kills per second |
| Serialization size | ~5 KB | 55 entities × ~90 bytes each |

---

## Recommendations for Stage 4

1. **Fix physics feedback loop** — Add proxy→ECS reverse sync in
   `EcsScene::update_physics()` or provide explicit `sync_back()`.
2. **Expose proxy→EntityId mapping** — Add public accessor on
   `EcsPhysicsAdapter` or store EntityId in tree Entity user-data.
3. **Add batch entity destroy** — `EcsScene::destroy_batch()` to amortize
   cleanup under high churn.
4. **Consider RenderQueue-based rendering for EcsScene** — Reduce uniform
   upload overhead by using the existing queue + culling path.

---

## How to Run

```bash
cd build_vs
cmake --build . --target micro_arena --config Release
bin/Release/micro_arena.exe
```

**Controls**: WASD move, Left Click shoot, F2 toggle profiler, F3 toggle physics debug.
