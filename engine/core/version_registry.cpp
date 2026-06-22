#include "engine/core/version_registry.h"

namespace pino {

void VersionRegistry::registerVersion(uint32_t type_id, uint32_t version, VersionLoader loader) {
    VersionKey key{type_id, version};
    m_loaders[key] = loader;
}

void VersionRegistry::dispatch(uint32_t type_id, uint32_t version, Deserializer& d) {
    VersionKey key{type_id, version};
    auto it = m_loaders.find(key);
    if (it != m_loaders.end()) {
        it->second(d);
    }
}

bool VersionRegistry::supports(uint32_t type_id, uint32_t version) const {
    VersionKey key{type_id, version};
    return m_loaders.find(key) != m_loaders.end();
}

} // namespace pino
