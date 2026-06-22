#include "engine/serialization/save_game_serializer.h"
#include "engine/assets/asset_manager.h"
#include <glm/gtc/quaternion.hpp>
#include <unordered_map>

namespace pino {

SaveGameSerializer::SaveGameSerializer(TypeRegistry& types, VersionRegistry& versions, StringTable& strings)
    : m_types(types)
    , m_versions(versions)
    , m_strings(strings)
{
}

void SaveGameSerializer::registerTypes(TypeRegistry& types) {
    types.registerType("SaveScene");
    types.registerType("SaveEntity");
}

static void loadSceneV1(Deserializer& d) {
    (void)d;
}

void SaveGameSerializer::registerVersions(VersionRegistry& versions) {
    versions.registerVersion(kSceneChunkType, kSceneVersion, loadSceneV1);
}

void SaveGameSerializer::writeEntity(Serializer& s, EntityId id, EcsScene& scene) {
    s.beginChunk(kEntityChunkType, kSceneVersion);

    s.writeUInt32(id.index);
    s.writeUInt32(id.generation);

    bool has_transform = scene.scene_graph().has(id);
    s.writeBool(has_transform);
    if (has_transform) {
        const Transform* t = scene.scene_graph().get(id);
        s.writeVec3(t->position);
        s.writeFloat(glm::angle(t->rotation));
        glm::vec3 axis = glm::axis(t->rotation);
        s.writeVec3(axis);
        s.writeVec3(t->scale);

        EntityId parent = scene.scene_graph().parent(id);
        s.writeUInt32(parent.index);
    }

    bool has_render = scene.has_component<RenderComponent>(id);
    s.writeBool(has_render);
    if (has_render) {
        const RenderComponent* rc = scene.get_component<RenderComponent>(id);
        (void)rc;
        uint32_t path_idx = m_strings.addString("");
        s.writeUInt32(path_idx);
    }

    bool has_physics = scene.has_component<PhysicsComponent>(id);
    s.writeBool(has_physics);
    if (has_physics) {
        const PhysicsComponent* pc = scene.get_component<PhysicsComponent>(id);
        s.writeVec3(pc->local_min);
        s.writeVec3(pc->local_max);
        s.writeBool(pc->is_static);
        s.writeUInt32(pc->collision_layer);
        s.writeUInt32(pc->collision_mask);
        s.writeVec3(pc->velocity);
    }

    bool has_audio = scene.has_component<AudioComponent>(id);
    s.writeBool(has_audio);
    if (has_audio) {
        const AudioComponent* ac = scene.get_component<AudioComponent>(id);
        uint32_t path_idx = m_strings.addString(ac->sound_path);
        s.writeUInt32(path_idx);
        s.writeFloat(ac->volume);
        s.writeBool(ac->looping);
        s.writeBool(ac->spatial);
        s.writeInt32(ac->attenuation_model);
        s.writeFloat(ac->min_dist);
        s.writeFloat(ac->max_dist);
        s.writeFloat(ac->rolloff);
    }

    s.endChunk();
}

void SaveGameSerializer::serialize(Serializer& s, EcsScene& scene) {
    s.beginChunk(kSceneChunkType, kSceneVersion);

    uint32_t entity_count = scene.entity_count();
    s.writeUInt32(entity_count);

    scene.registry().each([&](EntityId id) {
        writeEntity(s, id, scene);
    });

    s.endChunk();
}

EntityId SaveGameSerializer::readEntity(const SaveEntityData& data, EcsScene& scene, AssetManager* assets) {
    EntityId id = scene.create_entity();

    if (data.has_transform) {
        scene.scene_graph().attach(id, NullEntity);
        Transform* t = scene.scene_graph().get(id);
        t->position = data.position;
        t->rotation = data.rotation;
        t->scale = data.scale;
    }

    if (data.has_render) {
        RenderComponent& rc = scene.add_component<RenderComponent>(id);
        rc.material = nullptr;
        rc.transparent = false;
        rc.enabled = true;
        if (assets && !data.mesh_path.empty()) {
            rc.mesh = assets->get_mesh(data.mesh_path.c_str());
        }
    }

    if (data.has_physics) {
        PhysicsComponent& pc = scene.add_component<PhysicsComponent>(id);
        pc.local_min = data.physics_local_min;
        pc.local_max = data.physics_local_max;
        pc.is_static = data.physics_static;
        pc.enabled = true;
        pc.collision_layer = data.physics_layer;
        pc.collision_mask = data.physics_mask;
        pc.velocity = data.velocity;
    }

    if (data.has_audio) {
        AudioComponent& ac = scene.add_component<AudioComponent>(id);
        ac.sound_path = data.sound_path;
        ac.volume = data.audio_volume;
        ac.looping = data.audio_looping;
        ac.spatial = data.audio_spatial;
        ac.attenuation_model = data.audio_atten_model;
        ac.min_dist = data.audio_min_dist;
        ac.max_dist = data.audio_max_dist;
        ac.rolloff = data.audio_rolloff;
        ac.source_id = 0;
    }

    return id;
}

void SaveGameSerializer::deserialize(Deserializer& d, EcsScene& scene, AssetManager* assets) {
    scene.clear();

    std::unordered_map<uint32_t, EntityId> index_map;

    while (d.nextChunk()) {
        uint32_t type_id = d.getHeader().type_id;
        uint32_t version = d.getHeader().version;

        if (m_versions.supports(type_id, version)) {
            m_versions.dispatch(type_id, version, d);
        }

        if (type_id == kSceneChunkType) {
            uint32_t entity_count = d.readUInt32();
            (void)entity_count;
        } else if (type_id == kEntityChunkType) {
            SaveEntityData data{};
            data.index = d.readUInt32();
            data.generation = d.readUInt32();

            data.has_transform = d.readBool();
            if (data.has_transform) {
                data.position = d.readVec3();
                float angle = d.readFloat();
                glm::vec3 axis = d.readVec3();
                data.rotation = glm::angleAxis(angle, axis);
                data.scale = d.readVec3();
                data.parent_index = d.readUInt32();
            }

            data.has_render = d.readBool();
            if (data.has_render) {
                uint32_t path_idx = d.readUInt32();
                data.mesh_path = m_strings.getString(path_idx);
            }

            data.has_physics = d.readBool();
            if (data.has_physics) {
                data.physics_local_min = d.readVec3();
                data.physics_local_max = d.readVec3();
                data.physics_static = d.readBool();
                data.physics_layer = d.readUInt32();
                data.physics_mask = d.readUInt32();
                data.velocity = d.readVec3();
            }

            data.has_audio = d.readBool();
            if (data.has_audio) {
                uint32_t path_idx = d.readUInt32();
                data.sound_path = m_strings.getString(path_idx);
                data.audio_volume = d.readFloat();
                data.audio_looping = d.readBool();
                data.audio_spatial = d.readBool();
                data.audio_atten_model = d.readInt32();
                data.audio_min_dist = d.readFloat();
                data.audio_max_dist = d.readFloat();
                data.audio_rolloff = d.readFloat();
            }

            EntityId new_id = readEntity(data, scene, assets);
            index_map[data.index] = new_id;
        } else {
            d.skipChunk();
        }
    }

    scene.registry().each([&](EntityId id) {
        if (scene.scene_graph().has(id)) {
            EntityId parent = scene.scene_graph().parent(id);
            if (parent != NullEntity && index_map.find(parent.index) != index_map.end()) {
                EntityId new_parent = index_map[parent.index];
                scene.scene_graph().set_parent(id, new_parent);
            }
        }
    });
}

} // namespace pino
