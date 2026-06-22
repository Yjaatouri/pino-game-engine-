#pragma once

#include "engine/core/serializer.h"
#include "engine/core/type_registry.h"
#include "engine/core/version_registry.h"
#include "engine/core/string_table.h"
#include "engine/ecs/prefab.h"

namespace pino {

class PrefabSerializer {
public:
    PrefabSerializer(TypeRegistry& types, VersionRegistry& versions, StringTable& strings);

    static void registerTypes(TypeRegistry& types);
    static void registerVersions(VersionRegistry& versions);

    void serialize(Serializer& s, const Prefab& prefab);
    void deserialize(Deserializer& d, Prefab& prefab);

    static constexpr uint32_t kPrefabVersion      = 1;
    static constexpr uint32_t kTransformChunkType = 101;
    static constexpr uint32_t kComponentsChunkType = 102;
    static constexpr uint32_t kAssetsChunkType    = 103;

private:
    TypeRegistry& m_types;
    VersionRegistry& m_versions;
    StringTable& m_strings;
};

} // namespace pino
