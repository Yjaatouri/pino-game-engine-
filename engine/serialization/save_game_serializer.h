#pragma once

#include "engine/core/serializer.h"
#include "engine/core/type_registry.h"
#include "engine/core/version_registry.h"
#include "engine/core/string_table.h"
#include "engine/ecs/ecs_scene.h"
#include <functional>

namespace pino {

class AssetManager;

struct SaveEntityData {
    uint32_t index;
    uint32_t parent_index;

    bool has_transform;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;

    bool has_render;
    std::string mesh_path;

    bool has_physics;
    glm::vec3 physics_local_min;
    glm::vec3 physics_local_max;
    bool physics_static;
    uint32_t physics_layer;
    uint32_t physics_mask;
    glm::vec3 velocity;

    bool has_audio;
    std::string sound_path;
    float audio_volume;
    bool audio_looping;
    bool audio_spatial;
    int32_t audio_atten_model;
    float audio_min_dist;
    float audio_max_dist;
    float audio_rolloff;
};

class SaveGameSerializer {
public:
    // Progress callback: receives (entities_serialized_so_far, total_entities).
    using ProgressCallback = std::function<void(uint32_t current, uint32_t total)>;

    SaveGameSerializer(TypeRegistry& types, VersionRegistry& versions, StringTable& strings);

    static void registerTypes(TypeRegistry& types);
    static void registerVersions(VersionRegistry& versions);

    void serialize(Serializer& s, EcsScene& scene);
    void serialize(Serializer& s, EcsScene& scene, ProgressCallback cb);
    void deserialize(Deserializer& d, EcsScene& scene, AssetManager* assets = nullptr);

    static constexpr uint32_t kEntityChunkType = 301;
    static constexpr uint32_t kSceneVersion    = 1;

private:
    void writeEntity(Serializer& s, EntityId id, EcsScene& scene);
    EntityId readEntity(const SaveEntityData& data, EcsScene& scene, AssetManager* assets);

    TypeRegistry& m_types;
    VersionRegistry& m_versions;
    StringTable& m_strings;
};

} // namespace pino
