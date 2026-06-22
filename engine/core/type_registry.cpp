#include "engine/core/type_registry.h"
#include <cassert>
#include <cstdio>

namespace pino {

TypeRegistry::TypeRegistry()
    : m_next_id(1)
{
}

uint32_t TypeRegistry::registerType(const std::string& type_name) {
    assert(!type_name.empty() && "Type name must not be empty");

    auto it = m_name_to_id.find(type_name);
    if (it != m_name_to_id.end()) {
        assert(false && "Duplicate type registration");
        return it->second;
    }

    uint32_t id = m_next_id++;
    m_name_to_id[type_name] = id;
    m_id_to_name[id] = type_name;
    return id;
}

uint32_t TypeRegistry::getTypeID(const std::string& type_name) const {
    auto it = m_name_to_id.find(type_name);
    if (it == m_name_to_id.end()) {
        return 0;
    }
    return it->second;
}

const std::string& TypeRegistry::getTypeName(uint32_t type_id) const {
    static const std::string s_unknown = "UNKNOWN";
    auto it = m_id_to_name.find(type_id);
    if (it == m_id_to_name.end()) {
        return s_unknown;
    }
    return it->second;
}

bool TypeRegistry::isValid(uint32_t type_id) const {
    return m_id_to_name.find(type_id) != m_id_to_name.end();
}

void TypeRegistry::dumpAll() const {
    std::printf("=== TypeRegistry Dump ===\n");
    for (const auto& pair : m_id_to_name) {
        std::printf("  %4u -> %s\n", pair.first, pair.second.c_str());
    }
    std::printf("=========================\n");
}

} // namespace pino
