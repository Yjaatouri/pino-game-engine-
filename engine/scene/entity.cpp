#include "entity.h"
#include "engine/core/event_bus.h"
#include <algorithm>

namespace pino {

Entity::Entity(std::string name) : m_name(std::move(name)) {}

Entity::~Entity() {
    if (m_destroy_cb)
        m_destroy_cb(*this);
    EventBus::instance().emit(EntityDestroyedEvent{this});
}

glm::mat4 Entity::world_matrix() const {
    if (m_parent) {
        return m_parent->world_matrix() * m_local.matrix();
    }
    return m_local.matrix();
}

glm::vec3 Entity::world_position() const {
    return glm::vec3(world_matrix()[3]);
}

// ---- Tags ----

void Entity::add_tag(const std::string& tag) {
    if (tag.empty()) return;
    auto it = std::find(m_tags.begin(), m_tags.end(), tag);
    if (it == m_tags.end())
        m_tags.push_back(tag);
}

bool Entity::has_tag(const std::string& tag) const {
    return std::find(m_tags.begin(), m_tags.end(), tag) != m_tags.end();
}

void Entity::remove_tag(const std::string& tag) {
    auto it = std::find(m_tags.begin(), m_tags.end(), tag);
    if (it != m_tags.end())
        m_tags.erase(it);
}

// ---- Hierarchy ----

Entity* Entity::create_child(const std::string& name) {
    auto child = std::make_unique<Entity>(name);
    child->m_parent = this;
    Entity* ptr = child.get();
    m_children.push_back(std::move(child));
    return ptr;
}

void Entity::detach() {
    if (!m_parent) return;
    auto& siblings = m_parent->m_children;
    auto it = std::find_if(siblings.begin(), siblings.end(),
                           [this](const auto& p) { return p.get() == this; });
    if (it != siblings.end()) {
        siblings.erase(it);
    }
    m_parent = nullptr;
}

void Entity::clear_children() {
    m_children.clear();
}

void Entity::world_aabb(glm::vec3& out_min, glm::vec3& out_max) const {
    out_min = { FLT_MAX, FLT_MAX, FLT_MAX };
    out_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    if (!m_has_aabb) return;

    glm::vec3 corners[8] = {
        {m_aabb_min.x, m_aabb_min.y, m_aabb_min.z},
        {m_aabb_max.x, m_aabb_min.y, m_aabb_min.z},
        {m_aabb_min.x, m_aabb_max.y, m_aabb_min.z},
        {m_aabb_max.x, m_aabb_max.y, m_aabb_min.z},
        {m_aabb_min.x, m_aabb_min.y, m_aabb_max.z},
        {m_aabb_max.x, m_aabb_min.y, m_aabb_max.z},
        {m_aabb_min.x, m_aabb_max.y, m_aabb_max.z},
        {m_aabb_max.x, m_aabb_max.y, m_aabb_max.z},
    };

    glm::mat4 m = world_matrix();
    for (i32 i = 0; i < 8; ++i) {
        glm::vec4 c = m * glm::vec4(corners[i], 1.0f);
        glm::vec3 wp = glm::vec3(c);
        if (std::isnan(wp.x) || std::isnan(wp.y) || std::isnan(wp.z))
            continue;
        out_min = glm::min(out_min, wp);
        out_max = glm::max(out_max, wp);
    }
}

Entity* Entity::find_child(const std::string& name) {
    for (auto& c : m_children) {
        if (c->m_name == name) return c.get();
        auto* found = c->find_child(name);
        if (found) return found;
    }
    return nullptr;
}

const Entity* Entity::find_child(const std::string& name) const {
    for (const auto& c : m_children) {
        if (c->m_name == name) return c.get();
        auto* found = c->find_child(name);
        if (found) return found;
    }
    return nullptr;
}

} // namespace pino
