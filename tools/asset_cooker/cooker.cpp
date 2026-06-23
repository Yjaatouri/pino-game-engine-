#include "cooker.h"

namespace pino {

bool ICooker::accepts(const std::string& ext) const {
    return ext == extension();
}

void CookerRegistry::register_cooker(ICooker* cooker) {
    m_cookers.push_back(cooker);
    m_ext_map[cooker->extension()] = cooker;
}

ICooker* CookerRegistry::find(const std::string& ext) const {
    auto it = m_ext_map.find(ext);
    if (it != m_ext_map.end())
        return it->second;
    for (auto* c : m_cookers) {
        if (c->accepts(ext))
            return c;
    }
    return nullptr;
}

void register_all_cookers(CookerRegistry& reg) {
    register_mesh_cooker(reg);
    register_texture_cooker(reg);
    register_shader_cooker(reg);
}

} // namespace pino
