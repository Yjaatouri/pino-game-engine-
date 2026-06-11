#pragma once

#include "engine/scene/entity.h"
#include "engine/core/math_utils.h"
#include <vector>

namespace pino {

struct RaycastHit {
    Entity*     entity = nullptr;
    f32         t      = 0.0f;
    glm::vec3   point  = {0,0,0};
    glm::vec3   normal = {0,1,0}; // face normal (if available)
};

class Scene {
public:
    Scene();
    ~Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    Entity*       root()       { return &m_root; }
    const Entity* root() const { return &m_root; }

    // Convenience: findByName on root
    Entity*       find_by_name(const std::string& name)       { return m_root.find_child(name); }
    const Entity* find_by_name(const std::string& name) const { return m_root.find_child(name); }

    // Query all entities with a given tag
    std::vector<Entity*> find_all_with_tag(const std::string& tag);

    // Raycast against all registered colliders (active entities only)
    // Returns true if anything was hit; hit_out is populated with nearest hit.
    bool raycast(const Ray& ray, RaycastHit& hit_out) const;

    // Traversal (whole tree)
    template <typename F>
    void for_each(F&& func) {
        m_root.for_each(std::forward<F>(func));
    }

    template <typename F>
    void for_each(F&& func) const {
        m_root.for_each(std::forward<F>(func));
    }

    // Traverse only active entities
    template <typename F>
    void for_each_active(F&& func) {
        m_root.for_each_active(std::forward<F>(func));
    }

    void clear();

private:
    Entity m_root;
};

} // namespace pino
