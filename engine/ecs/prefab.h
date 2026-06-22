#pragma once

#include "engine/ecs/ecs_scene.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/transform.h"
#include <type_traits>
#include <string>
#include <vector>

namespace pino {

class EcsWorld;
class PrefabSerializer;

// Compile-time FNV-1a hash for component type identification.
namespace detail {
    constexpr u32 fnv1a(const char* s, u32 h = 0x811c9dc5u) {
        return *s ? fnv1a(s + 1, static_cast<u32>((static_cast<u8>(*s) ^ h) * 0x01000193u)) : h;
    }
}

static constexpr u32 kRenderComponentHash  = detail::fnv1a("RenderComponent");
static constexpr u32 kPhysicsComponentHash = detail::fnv1a("PhysicsComponent");
static constexpr u32 kAudioComponentHash   = detail::fnv1a("AudioComponent");

// A prefab is a serializable blueprint for an entity + its components + asset references.
//
// Usage (authoring):
//   Prefab player;
//   player.set_transform({0,0,0}, {}, {1,1,1});
//   player.set_component(PhysicsComponent{...});
//   player.set_mesh("models/player.obj");
//   player.set_sound("sounds/footstep.wav");
//   player.save("prefabs/player.prefab");
//
// Usage (runtime):
//   Prefab player;
//   player.load("prefabs/player.prefab");
//   EntityId e = player.instantiate(scene, assets);
class Prefab {
    friend class PrefabSerializer;
public:
    Prefab() = default;

    // ── Authoring ────────────────────────────────────────────────

    void set_transform(const glm::vec3& pos, const glm::quat& rot = {}, const glm::vec3& scl = {1,1,1});
    void clear_transform();

    template <typename T>
    void set_component(const T& comp) {
        ComponentEntry entry;
        entry.type_hash = type_hash_for<T>();
        entry.data.resize(sizeof(T));
        memcpy(entry.data.data(), &comp, sizeof(T));
        // Replace existing entry of same type, or append.
        for (auto& e : m_components) {
            if (e.type_hash == entry.type_hash) { e.data.swap(entry.data); return; }
        }
        m_components.push_back(std::move(entry));
    }

    void set_mesh(const std::string& path)    { m_mesh_path = path; }
    void set_sound(const std::string& path)   { m_sound_path = path; }
    void clear_mesh()  { m_mesh_path.clear(); }
    void clear_sound() { m_sound_path.clear(); }

    // ── Public type for component data ─────────────────────────
    struct ComponentEntry {
        u32 type_hash = 0;
        std::vector<u8> data;
    };

    const std::string& mesh_path()  const { return m_mesh_path; }
    const std::string& sound_path() const { return m_sound_path; }

    // Inspection (for serialization/editor tools).
    bool has_transform() const { return m_has_transform; }
    const Transform& transform() const { return m_transform; }
    const std::vector<ComponentEntry>& components() const { return m_components; }

    // Create an entity in the scene with all components.
    // Asset handles (mesh) are left empty — use resolve_assets() after instantiation
    // or call the AssetManager overload to resolve automatically.
    EntityId instantiate(EcsScene& scene, EntityId parent = NullEntity) const;

    // Create an entity with asset resolution via AssetManager.
    EntityId instantiate(EcsScene& scene, AssetManager& assets, EntityId parent = NullEntity) const;

    // Same as instantiate but into the EcsWorld (convenience).
    // Defined in prefab.cpp to avoid circular include of ecs_world.h.
    EntityId instantiate(EcsWorld& world, AssetManager& assets, EntityId parent = NullEntity) const;

    // ── Serialization ────────────────────────────────────────────

    // Binary save/load.
    bool save(const std::string& filepath) const;
    bool load(const std::string& filepath);

    // Raw binary access (for AssetPack integration or custom serialization).
    std::vector<u8> serialize() const;
    bool deserialize(const std::vector<u8>& data);

private:
    template <typename T> static u32 type_hash_for() {
        if constexpr (std::is_same_v<T, RenderComponent>)  return kRenderComponentHash;
        if constexpr (std::is_same_v<T, PhysicsComponent>)  return kPhysicsComponentHash;
        if constexpr (std::is_same_v<T, AudioComponent>)    return kAudioComponentHash;
        return 0;
    }

    bool m_has_transform = false;
    Transform m_transform;

    std::vector<ComponentEntry> m_components;

    std::string m_mesh_path;    // resolved → AssetHandle<Mesh> on RenderComponent
    std::string m_sound_path;   // stored into AudioComponent::sound_path
};

// ── Inline helpers ───────────────────────────────────────────────

inline void Prefab::set_transform(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scl) {
    m_has_transform = true;
    m_transform.position = pos;
    m_transform.rotation = rot;
    m_transform.scale = scl;
}

inline void Prefab::clear_transform() {
    m_has_transform = false;
    m_transform = {};
}

// ── Binary serialization format ─────────────────────────────────
// [magic  "PREF"] [version u32] [flags u32]
// [has_transform u8] [pos 12] [rot 16] [scl 12]
// [component_count u32]
//   for each: [type_hash u32] [data_size u32] [data]
// [mesh_path_len u16] [mesh_path]
// [sound_path_len u16] [sound_path]

} // namespace pino
