#pragma once

#include "engine/core/types.h"
#include "engine/core/transform.h"
#include "engine/core/math_utils.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>

namespace pino {

class Entity {
public:
    explicit Entity(std::string name);
    ~Entity();

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;

    Entity(Entity&&) = delete;
    Entity& operator=(Entity&&) = delete;

    // ---- Name ----
    const std::string& name() const { return m_name; }
    void set_name(const std::string& n) { m_name = n; }

    // ---- Active state ----
    void set_active(bool active) { m_active = active; }
    bool is_active() const { return m_active; }

    // ---- Tags ----
    void add_tag(const std::string& tag);
    bool has_tag(const std::string& tag) const;
    void remove_tag(const std::string& tag);
    const std::vector<std::string>& tags() const { return m_tags; }

    // ---- Transform ----
    Transform&       local_transform()       { return m_local; }
    const Transform& local_transform() const { return m_local; }

    glm::mat4 world_matrix()  const;
    glm::vec3 world_position() const;

    // ---- Hierarchy ----
    Entity* parent() const { return m_parent; }

    // Create a child (takes ownership)
    Entity* create_child(const std::string& name);

    // Detach from parent (parent releases ownership, entity is destroyed)
    void detach();

    // Remove and destroy all children
    void clear_children();

    // Find by name (recursive, returns first match or null)
    Entity*       find_child(const std::string& name);
    const Entity* find_child(const std::string& name) const;
    Entity*       find_by_name(const std::string& name)       { return find_child(name); }
    const Entity* find_by_name(const std::string& name) const { return find_child(name); }

    // Optional local-space AABB for raycasting / broad-phase
    void set_local_aabb(const glm::vec3& min, const glm::vec3& max) {
        m_aabb_min = min; m_aabb_max = max; m_has_aabb = true;
    }
    void clear_local_aabb() { m_has_aabb = false; }
    bool has_aabb() const { return m_has_aabb; }
    const glm::vec3& aabb_min() const { return m_aabb_min; }
    const glm::vec3& aabb_max() const { return m_aabb_max; }

    // Get world-space AABB (expensive — recomputes from transform)
    void world_aabb(glm::vec3& out_min, glm::vec3& out_max) const;

    // Destroy-self callback registration (for external cleanup, e.g. CollisionWorld)
    using DestroyCallback = std::function<void(Entity&)>;
    void on_destroy(DestroyCallback cb) { m_destroy_cb = std::move(cb); }

    // Opaque user-data (e.g. packed ECS EntityId for physics proxy mapping)
    void  set_user_data(uint64_t data) { m_user_data = data; }
    uint64_t user_data() const { return m_user_data; }

    // Number of direct children
    u32 child_count() const { return static_cast<u32>(m_children.size()); }

    // ---- Traversal ----
    // Safe: copies child pointers before iteration to prevent invalidation
    template <typename F>
    void for_each(F&& func) {
        func(*this);
        // Snapshot pointers to allow safe add/remove during iteration
        std::vector<Entity*> snap;
        snap.reserve(m_children.size());
        for (auto& c : m_children) snap.push_back(c.get());
        for (auto* child : snap) {
            if (child->m_parent == this) // still attached
                child->for_each(std::forward<F>(func));
        }
    }

    template <typename F>
    void for_each(F&& func) const {
        func(*this);
        std::vector<const Entity*> snap;
        snap.reserve(m_children.size());
        for (const auto& c : m_children) snap.push_back(c.get());
        for (const auto* child : snap) {
            if (child->m_parent == this)
                child->for_each(std::forward<F>(func));
        }
    }

    // Traverse only active entities
    template <typename F>
    void for_each_active(F&& func) {
        if (!m_active) return;
        func(*this);
        std::vector<Entity*> snap;
        snap.reserve(m_children.size());
        for (auto& c : m_children) snap.push_back(c.get());
        for (auto* child : snap) {
            if (child->m_parent == this)
                child->for_each_active(std::forward<F>(func));
        }
    }

private:
    std::string m_name;
    Transform   m_local;
    bool        m_active = true;
    std::vector<std::string> m_tags;

    glm::vec3 m_aabb_min = {0,0,0};
    glm::vec3 m_aabb_max = {0,0,0};
    bool      m_has_aabb = false;

    Entity* m_parent = nullptr;
    std::vector<std::unique_ptr<Entity>> m_children;
    DestroyCallback m_destroy_cb;
    uint64_t m_user_data = 0;
};

} // namespace pino
