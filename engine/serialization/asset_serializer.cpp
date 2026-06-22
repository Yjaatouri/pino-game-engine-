#include "engine/serialization/asset_serializer.h"

namespace pino {

AssetSerializer::AssetSerializer(TypeRegistry& types, VersionRegistry& versions, StringTable& strings)
    : m_types(types)
    , m_versions(versions)
    , m_strings(strings)
{
}

void AssetSerializer::registerTypes(TypeRegistry& types) {
    types.registerType("AssetMeta");
}

static void loadAssetV1(Deserializer& d) {
    (void)d;
}

void AssetSerializer::registerVersions(VersionRegistry& versions) {
    versions.registerVersion(kAssetChunkType, kAssetVersion, loadAssetV1);
}

void AssetSerializer::serialize(Serializer& s, const AssetMeta& asset) {
    s.beginChunk(kAssetChunkType, kAssetVersion);

    uint32_t path_idx = m_strings.addString(asset.path);
    s.writeUInt32(path_idx);
    s.writeUInt32(static_cast<uint32_t>(asset.type));
    s.writeUInt32(asset.version);

    if (asset.type == AssetType::Shader) {
        uint32_t vert_idx = m_strings.addString(asset.shader_vert);
        uint32_t frag_idx = m_strings.addString(asset.shader_frag);
        s.writeUInt32(vert_idx);
        s.writeUInt32(frag_idx);
    }

    s.endChunk();
}

void AssetSerializer::deserialize(Deserializer& d, AssetMeta& asset) {
    while (d.nextChunk()) {
        uint32_t type_id = d.getHeader().type_id;
        uint32_t version = d.getHeader().version;

        if (m_versions.supports(type_id, version)) {
            m_versions.dispatch(type_id, version, d);
        }

        if (type_id == kAssetChunkType && version == kAssetVersion) {
            uint32_t path_idx = d.readUInt32();
            asset.path = m_strings.getString(path_idx);
            asset.type = static_cast<AssetType>(d.readUInt32());
            asset.version = d.readUInt32();

            if (asset.type == AssetType::Shader) {
                uint32_t vert_idx = d.readUInt32();
                uint32_t frag_idx = d.readUInt32();
                asset.shader_vert = m_strings.getString(vert_idx);
                asset.shader_frag = m_strings.getString(frag_idx);
            }
        } else {
            d.skipChunk();
        }
    }
}

} // namespace pino
