#pragma once

#include "engine/core/serializer.h"
#include "engine/core/type_registry.h"
#include "engine/core/version_registry.h"
#include "engine/core/string_table.h"
#include <string>

namespace pino {

enum class AssetType : uint32_t {
    Mesh = 0,
    Texture,
    Shader,
    AudioClip,
    Other
};

struct AssetMeta {
    std::string path;
    AssetType type;
    uint32_t version;

    // Extras
    std::string shader_vert;
    std::string shader_frag;
};

class AssetSerializer {
public:
    AssetSerializer(TypeRegistry& types, VersionRegistry& versions, StringTable& strings);

    static void registerTypes(TypeRegistry& types);
    static void registerVersions(VersionRegistry& versions);

    void serialize(Serializer& s, const AssetMeta& asset);
    void deserialize(Deserializer& d, AssetMeta& asset);

    static constexpr uint32_t kAssetChunkType = 200;
    static constexpr uint32_t kAssetVersion    = 1;

private:
    TypeRegistry& m_types;
    VersionRegistry& m_versions;
    StringTable& m_strings;
};

} // namespace pino
