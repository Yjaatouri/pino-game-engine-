#include "engine/serialization/prefab_serializer.h"
#include <glm/gtc/quaternion.hpp>

namespace pino {

PrefabSerializer::PrefabSerializer(TypeRegistry& types, VersionRegistry& versions, StringTable& strings)
    : m_types(types)
    , m_versions(versions)
    , m_strings(strings)
{
}

void PrefabSerializer::registerTypes(TypeRegistry& types) {
    types.registerType("PrefabTransform");
    types.registerType("PrefabComponents");
    types.registerType("PrefabAssets");
}

static void loadPrefabV1(Deserializer& d) {
    (void)d;
}

void PrefabSerializer::registerVersions(VersionRegistry& versions) {
    versions.registerVersion(kPrefabChunkType, kPrefabVersion, loadPrefabV1);
}

void PrefabSerializer::serialize(Serializer& s, const Prefab& prefab) {
    s.beginChunk(kPrefabChunkType, kPrefabVersion);

    s.beginChunk(kTransformChunkType, kPrefabVersion);
    s.writeBool(prefab.has_transform());
    if (prefab.has_transform()) {
        const auto& t = prefab.transform();
        s.writeVec3(t.position);
        s.writeFloat(glm::angle(t.rotation));
        glm::vec3 axis = glm::axis(t.rotation);
        s.writeVec3(axis);
        s.writeVec3(t.scale);
    }
    s.endChunk();

    s.beginChunk(kComponentsChunkType, kPrefabVersion);
    const auto& components = prefab.components();
    s.writeUInt32(static_cast<uint32_t>(components.size()));
    for (const auto& entry : components) {
        s.writeUInt32(entry.type_hash);
        s.writeUInt32(static_cast<uint32_t>(entry.data.size()));
        if (!entry.data.empty()) {
            s.writeBytes(entry.data.data(), static_cast<uint32_t>(entry.data.size()));
        }
    }
    s.endChunk();

    s.beginChunk(kAssetsChunkType, kPrefabVersion);
    uint32_t mesh_idx = m_strings.addString(prefab.mesh_path());
    uint32_t sound_idx = m_strings.addString(prefab.sound_path());
    s.writeUInt32(mesh_idx);
    s.writeUInt32(sound_idx);
    s.endChunk();

    s.endChunk();
}

void PrefabSerializer::deserialize(Deserializer& d, Prefab& prefab) {
    while (d.nextChunk()) {
        uint32_t type_id = d.getHeader().type_id;
        uint32_t version = d.getHeader().version;

        if (m_versions.supports(type_id, version)) {
            m_versions.dispatch(type_id, version, d);
        }

        if (type_id == kTransformChunkType) {
            bool has_transform = d.readBool();
            if (has_transform) {
                glm::vec3 pos = d.readVec3();
                float angle = d.readFloat();
                glm::vec3 axis = d.readVec3();
                glm::vec3 scl = d.readVec3();
                glm::quat rot = glm::angleAxis(angle, axis);
                prefab.set_transform(pos, rot, scl);
            }
        } else if (type_id == kComponentsChunkType) {
            uint32_t count = d.readUInt32();
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t type_hash = d.readUInt32();
                uint32_t data_size = d.readUInt32();
                Prefab::ComponentEntry entry;
                entry.type_hash = type_hash;
                entry.data.resize(data_size);
                if (data_size > 0) {
                    d.readBytes(entry.data.data(), data_size);
                }
                prefab.m_components.push_back(std::move(entry));
            }
        } else if (type_id == kAssetsChunkType) {
            uint32_t mesh_idx = d.readUInt32();
            uint32_t sound_idx = d.readUInt32();
            prefab.m_mesh_path = m_strings.getString(mesh_idx);
            prefab.m_sound_path = m_strings.getString(sound_idx);
        } else {
            d.skipChunk();
        }
    }
}

} // namespace pino
