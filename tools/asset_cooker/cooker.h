#pragma once

#include "engine/core/binary_chunk.h"
#include "engine/serialization/cooked_asset.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace pino {

struct CookResult {
    bool        ok    = false;
    std::string error;
};

struct CookInput {
    std::string source_path;
    std::string asset_name;
    std::string identifier;
};

class ICooker {
public:
    virtual ~ICooker() = default;
    virtual std::string extension() const = 0;
    virtual bool accepts(const std::string& ext) const;
    virtual CookResult cook(const CookInput& input, BinaryChunkWriter& writer) = 0;
};

class CookerRegistry {
public:
    void register_cooker(ICooker* cooker);
    ICooker* find(const std::string& ext) const;
    const std::vector<ICooker*>& all() const { return m_cookers; }
private:
    std::unordered_map<std::string, ICooker*> m_ext_map;
    std::vector<ICooker*> m_cookers;
};

void register_mesh_cooker(CookerRegistry& reg);
void register_texture_cooker(CookerRegistry& reg);
void register_shader_cooker(CookerRegistry& reg);
void register_all_cookers(CookerRegistry& reg);

} // namespace pino
