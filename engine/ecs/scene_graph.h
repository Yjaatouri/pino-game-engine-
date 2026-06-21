#pragma once

#include "engine/ecs/entity.h"
#include "engine/ecs/entity_registry.h"
#include "engine/core/transform.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <vector>

namespace pino {

// SceneGraph manages per-entity transforms with parent-child hierarchy,
// dirty-flag world matrix computation, and cycle-safe reparenting.
//
// Usage:
//   EntityRegistry reg;
//   SceneGraph sg(&reg);
//   auto child = sg.attach(sg.attach(reg.create(), NullEntity), parent);
//   sg.set_position(child, {1,0,0});
//   glm::mat4 world = sg.world_matrix(child);
//
// The SceneGraph does NOT hook into EntityRegistry destruction automatically.
// Call sg.destroy(entity) before (or instead of) reg.destroy(entity) to
// ensure children are properly orphaned first.
class SceneGraph {
public:
    explicit SceneGraph(EntityRegistry* registry)
        : m_registry(registry) {}

    SceneGraph(const SceneGraph&) = delete;
    SceneGraph& operator=(const SceneGraph&) = delete;

    // ── Lifecycle ────────────────────────────────────────────────

    // Attach a transform to an entity, optionally parenting it.
    // Returns true if successful (entity was valid and didn't already have a transform).
    bool attach(EntityId entity, EntityId parent = NullEntity);

    // Detach an entity's transform and orphan its children.
    void detach(EntityId entity);

    // Detach the entity (orphaning children) and destroy it in the registry.
    void destroy(EntityId entity);

    // Returns true if the entity has a transform node.
    bool has(EntityId entity) const;

    // Number of entities with transforms.
    u32 count() const { return m_count; }

    // Remove all transforms and hierarchy.
    void clear();

    // ── Hierarchy queries ────────────────────────────────────────

    // Reparent an entity (with cycle detection). Returns false if cycle detected.
    bool set_parent(EntityId entity, EntityId new_parent);

    // Get the parent, or NullEntity if root-level.
    EntityId parent(EntityId entity) const;

    // Read-only child list for the entity.
    const std::vector<EntityId>& children(EntityId entity) const;

    // Number of direct children.
    u32 child_count(EntityId entity) const;

    // ── Transform access ─────────────────────────────────────────

    // Get a mutable pointer to the local Transform (modifies, marks dirty).
    Transform* get(EntityId entity);

    // Convenience setters (mark dirty automatically).
    void set_position(EntityId entity, const glm::vec3& pos);
    void set_rotation(EntityId entity, const glm::quat& rot);
    void set_scale(EntityId entity, const glm::vec3& scl);

    // Cached world matrix, recomputed if dirty.
    glm::mat4 world_matrix(EntityId entity);

    // Derived world-space properties (recomputed from world_matrix).
    glm::vec3 world_position(EntityId entity);
    glm::quat world_rotation(EntityId entity);
    glm::vec3 world_scale(EntityId entity);

    // ── Iteration ────────────────────────────────────────────────

    // Iterate all entities that have transforms.
    template <typename F>
    void each(F&& func) {
        for (u32 i = 0; i < static_cast<u32>(m_nodes.size()); ++i) {
            if (m_nodes[i].active) {
                EntityId e{i, m_nodes[i].entity_generation};
                func(e);
            }
        }
    }

    template <typename F>
    void each(F&& func) const {
        for (u32 i = 0; i < static_cast<u32>(m_nodes.size()); ++i) {
            if (m_nodes[i].active) {
                EntityId e{i, m_nodes[i].entity_generation};
                func(e);
            }
        }
    }

private:
    struct Node {
        Transform local;
        glm::mat4 world = glm::mat4(1.0f);
        bool dirty = true;

        EntityId parent = NullEntity;
        std::vector<EntityId> children;

        u32 entity_generation = 0;
        bool active = false;
        u32 next_free = UINT32_MAX;
    };

    EntityRegistry* m_registry;
    std::vector<Node> m_nodes;
    u32 m_free_head = UINT32_MAX;
    u32 m_count = 0;

    // Sparse: entity.index -> node index (or UINT32_MAX if none).
    // Resized on attach; entries for dead entities become stale (detected
    // via generation mismatch in the node).
    std::vector<u32> m_entity_to_node;

    u32 alloc_slot();
    void free_slot(u32 idx);
    u32 find_node(EntityId entity) const;
    Node* node_for(EntityId entity);
    const Node* node_for(EntityId entity) const;

    void mark_dirty(u32 node_idx);
    glm::mat4 compute_world(u32 node_idx);
    bool would_cycle(EntityId entity, EntityId new_parent) const;
};

// ── Inline implementation ───────────────────────────────────────

inline bool SceneGraph::attach(EntityId entity, EntityId parent) {
    if (!m_registry || !m_registry->alive(entity)) return false;
    if (has(entity)) return false;

    if (parent && !m_registry->alive(parent)) parent = NullEntity;
    if (parent && would_cycle(entity, parent)) return false;

    u32 idx = alloc_slot();
    auto& node = m_nodes[idx];
    node.active = true;
    node.entity_generation = entity.generation;
    node.dirty = true;
    node.parent = NullEntity;

    // Ensure mapping is large enough
    if (entity.index >= m_entity_to_node.size())
        m_entity_to_node.resize(entity.index + 1, UINT32_MAX);
    m_entity_to_node[entity.index] = idx;

    ++m_count;

    if (parent) {
        node.parent = parent;
        u32 p_idx = find_node(parent);
        if (p_idx != UINT32_MAX) {
            m_nodes[p_idx].children.push_back(entity);
            mark_dirty(idx); // propagate from parent
        }
    }

    return true;
}

inline void SceneGraph::detach(EntityId entity) {
    u32 idx = find_node(entity);
    if (idx == UINT32_MAX) return;
    auto& node = m_nodes[idx];

    // Orphan children (set their parent to null)
    for (auto child_id : node.children) {
        u32 c_idx = find_node(child_id);
        if (c_idx != UINT32_MAX) {
            m_nodes[c_idx].parent = NullEntity;
            mark_dirty(c_idx);
        }
    }
    node.children.clear();

    // Remove self from parent's child list
    if (node.parent) {
        u32 p_idx = find_node(node.parent);
        if (p_idx != UINT32_MAX) {
            auto& siblings = m_nodes[p_idx].children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), entity),
                           siblings.end());
        }
    }

    // Invalidate mapping
    if (entity.index < m_entity_to_node.size())
        m_entity_to_node[entity.index] = UINT32_MAX;

    free_slot(idx);
}

inline void SceneGraph::destroy(EntityId entity) {
    detach(entity);
    if (m_registry) m_registry->destroy(entity);
}

inline bool SceneGraph::has(EntityId entity) const {
    return find_node(entity) != UINT32_MAX;
}

inline void SceneGraph::clear() {
    m_nodes.clear();
    m_entity_to_node.clear();
    m_free_head = UINT32_MAX;
    m_count = 0;
}

inline bool SceneGraph::set_parent(EntityId entity, EntityId new_parent) {
    u32 idx = find_node(entity);
    if (idx == UINT32_MAX) return false;
    if (new_parent && !m_registry->alive(new_parent)) return false;
    if (would_cycle(entity, new_parent)) return false;

    auto& node = m_nodes[idx];

    // Remove from old parent
    if (node.parent) {
        u32 p_idx = find_node(node.parent);
        if (p_idx != UINT32_MAX) {
            auto& siblings = m_nodes[p_idx].children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), entity),
                           siblings.end());
        }
    }

    node.parent = new_parent;

    // Add to new parent
    if (new_parent) {
        u32 p_idx = find_node(new_parent);
        if (p_idx != UINT32_MAX) {
            m_nodes[p_idx].children.push_back(entity);
        }
    }

    mark_dirty(idx);
    return true;
}

inline EntityId SceneGraph::parent(EntityId entity) const {
    auto* node = node_for(entity);
    return node ? node->parent : NullEntity;
}

inline const std::vector<EntityId>& SceneGraph::children(EntityId entity) const {
    static const std::vector<EntityId> empty;
    auto* node = node_for(entity);
    return node ? node->children : empty;
}

inline u32 SceneGraph::child_count(EntityId entity) const {
    auto* node = node_for(entity);
    return node ? static_cast<u32>(node->children.size()) : 0;
}

inline Transform* SceneGraph::get(EntityId entity) {
    u32 idx = find_node(entity);
    if (idx == UINT32_MAX) return nullptr;
    mark_dirty(idx);
    return &m_nodes[idx].local;
}

inline void SceneGraph::set_position(EntityId entity, const glm::vec3& pos) {
    auto* t = get(entity);
    if (t) t->position = pos;
}

inline void SceneGraph::set_rotation(EntityId entity, const glm::quat& rot) {
    auto* t = get(entity);
    if (t) t->rotation = rot;
}

inline void SceneGraph::set_scale(EntityId entity, const glm::vec3& scl) {
    auto* t = get(entity);
    if (t) t->scale = scl;
}

inline glm::mat4 SceneGraph::world_matrix(EntityId entity) {
    u32 idx = find_node(entity);
    if (idx == UINT32_MAX) return glm::mat4(1.0f);
    return compute_world(idx);
}

inline glm::vec3 SceneGraph::world_position(EntityId entity) {
    return glm::vec3(world_matrix(entity)[3]);
}

inline glm::quat SceneGraph::world_rotation(EntityId entity) {
    return glm::quat_cast(world_matrix(entity));
}

inline glm::vec3 SceneGraph::world_scale(EntityId entity) {
    auto m = world_matrix(entity);
    return {
        glm::length(glm::vec3(m[0])),
        glm::length(glm::vec3(m[1])),
        glm::length(glm::vec3(m[2]))
    };
}

// ── Private helpers ─────────────────────────────────────────────

inline u32 SceneGraph::alloc_slot() {
    if (m_free_head != UINT32_MAX) {
        u32 idx = m_free_head;
        m_free_head = m_nodes[idx].next_free;
        m_nodes[idx] = Node{};
        return idx;
    }
    m_nodes.push_back({});
    return static_cast<u32>(m_nodes.size()) - 1;
}

inline void SceneGraph::free_slot(u32 idx) {
    m_nodes[idx].active = false;
    m_nodes[idx].next_free = m_free_head;
    m_free_head = idx;
    --m_count;
}

inline u32 SceneGraph::find_node(EntityId entity) const {
    if (entity.index >= m_entity_to_node.size()) return UINT32_MAX;
    u32 idx = m_entity_to_node[entity.index];
    if (idx == UINT32_MAX) return UINT32_MAX;
    const auto& node = m_nodes[idx];
    if (!node.active) return UINT32_MAX;
    if (node.entity_generation != entity.generation) return UINT32_MAX;
    if (m_registry && !m_registry->alive(entity)) return UINT32_MAX;
    return idx;
}

inline SceneGraph::Node* SceneGraph::node_for(EntityId entity) {
    u32 idx = find_node(entity);
    return (idx != UINT32_MAX) ? &m_nodes[idx] : nullptr;
}

inline const SceneGraph::Node* SceneGraph::node_for(EntityId entity) const {
    u32 idx = find_node(entity);
    return (idx != UINT32_MAX) ? &m_nodes[idx] : nullptr;
}

inline void SceneGraph::mark_dirty(u32 node_idx) {
    if (node_idx >= m_nodes.size() || !m_nodes[node_idx].active) return;
    m_nodes[node_idx].dirty = true;
    for (auto child_id : m_nodes[node_idx].children) {
        u32 c_idx = find_node(child_id);
        mark_dirty(c_idx);
    }
}

inline glm::mat4 SceneGraph::compute_world(u32 node_idx) {
    auto& node = m_nodes[node_idx];
    if (!node.dirty) return node.world;

    if (node.parent) {
        u32 p_idx = find_node(node.parent);
        if (p_idx != UINT32_MAX) {
            node.world = compute_world(p_idx) * node.local.matrix();
        } else {
            // Parent entity was destroyed — treat as root
            node.parent = NullEntity;
            node.world = node.local.matrix();
        }
    } else {
        node.world = node.local.matrix();
    }

    node.dirty = false;
    return node.world;
}

inline bool SceneGraph::would_cycle(EntityId entity, EntityId new_parent) const {
    EntityId cur = new_parent;
    while (cur) {
        if (cur == entity) return true;
        u32 idx = find_node(cur);
        if (idx == UINT32_MAX) break;
        cur = m_nodes[idx].parent;
    }
    return false;
}

} // namespace pino
