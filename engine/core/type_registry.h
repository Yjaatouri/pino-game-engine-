#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace pino {

struct TypeInfo {
    uint32_t id;
    std::string name;
};

class TypeRegistry {
public:
    TypeRegistry();

    uint32_t registerType(const std::string& type_name);

    uint32_t getTypeID(const std::string& type_name) const;
    const std::string& getTypeName(uint32_t type_id) const;

    bool isValid(uint32_t type_id) const;

    void dumpAll() const;

private:
    std::unordered_map<std::string, uint32_t> m_name_to_id;
    std::unordered_map<uint32_t, std::string> m_id_to_name;
    uint32_t m_next_id;
};

} // namespace pino
