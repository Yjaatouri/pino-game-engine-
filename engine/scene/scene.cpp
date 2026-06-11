#include "scene.h"
#include <cfloat>

namespace pino {

Scene::Scene() : m_root("__root__") {}

void Scene::clear() {
    m_root.clear_children();
}

std::vector<Entity*> Scene::find_all_with_tag(const std::string& tag) {
    std::vector<Entity*> result;
    m_root.for_each([&](Entity& e) {
        if (e.has_tag(tag))
            result.push_back(&e);
    });
    return result;
}

bool Scene::raycast(const Ray& ray, RaycastHit& hit_out) const {
    RaycastHit best;
    best.t = FLT_MAX;

    m_root.for_each([&](const Entity& e) {
        if (!e.is_active() || !e.has_aabb()) return;
        if (&e == &m_root) return;

        glm::vec3 wmin, wmax;
        e.world_aabb(wmin, wmax);

        f32 t;
        if (rayAABBIntersection(ray, wmin, wmax, t)) {
            if (t < best.t) {
                best.entity = const_cast<Entity*>(&e);
                best.t      = t;
                best.point  = ray.at(t);
            }
        }
    });

    if (best.entity) {
        hit_out = best;
        return true;
    }
    return false;
}

} // namespace pino
