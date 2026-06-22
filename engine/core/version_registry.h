#pragma once

#include "engine/core/serializer.h"
#include <cstdint>
#include <unordered_map>

namespace pino {

using VersionLoader = void(*)(Deserializer&);

struct VersionKey {
    uint32_t type_id;
    uint32_t version;

    bool operator==(const VersionKey& other) const {
        return type_id == other.type_id && version == other.version;
    }
};

struct VersionKeyHash {
    uint64_t operator()(const VersionKey& key) const {
        return (static_cast<uint64_t>(key.type_id) << 32) | key.version;
    }
};

class VersionRegistry {
public:
    void registerVersion(uint32_t type_id, uint32_t version, VersionLoader loader);

    void dispatch(uint32_t type_id, uint32_t version, Deserializer& d);

    bool supports(uint32_t type_id, uint32_t version) const;

private:
    std::unordered_map<VersionKey, VersionLoader, VersionKeyHash> m_loaders;
};

} // namespace pino
