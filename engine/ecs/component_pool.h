#pragma once

#include "engine/ecs/entity.h"
#include "engine/ecs/entity_registry.h"
#include <vector>

namespace pino {

// Packed component storage using a sparse→dense index mapping.
// Components are stored contiguously in m_dense for cache-friendly
// iteration. Swap-with-last removal keeps the dense array packed with
// no holes.
//
// Cross-checks with EntityRegistry to detect stale IDs from
// destroyed entities, even when the slot has not been reused.
//
// If set_registry() is called, get()/has() also verify that the
// entity is alive in the registry, preventing false positives
// after destruction.
template <typename T>
class ComponentPool {
public:
    static constexpr u32 SENTINEL = UINT32_MAX;

    ComponentPool() { m_dense.reserve(64); }

    // Optional registry for cross-checking entity liveness.
    void set_registry(const EntityRegistry* reg) { m_registry = reg; }

    // Returns pointer to component if entity has one, else nullptr.
    T* get(EntityId entity) {
        if (entity.index >= m_sparse.size()) return nullptr;
        u32 dense_idx = m_sparse[entity.index];
        if (dense_idx == SENTINEL) return nullptr;
        auto& slot = m_dense[dense_idx];
        if (slot.id.generation != entity.generation) return nullptr;
        if (m_registry && !m_registry->alive(entity)) return nullptr;
        return &slot.data;
    }

    const T* get(EntityId entity) const {
        if (entity.index >= m_sparse.size()) return nullptr;
        u32 dense_idx = m_sparse[entity.index];
        if (dense_idx == SENTINEL) return nullptr;
        const auto& slot = m_dense[dense_idx];
        if (slot.id.generation != entity.generation) return nullptr;
        if (m_registry && !m_registry->alive(entity)) return nullptr;
        return &slot.data;
    }

    bool has(EntityId entity) const {
        return get(entity) != nullptr;
    }

    // Add a component (default-constructed). Returns reference.
    // If the entity already has a component, it is overwritten.
    T& add(EntityId entity) {
        if (entity.index >= m_sparse.size())
            m_sparse.resize(entity.index + 1, SENTINEL);

        u32 dense_idx = m_sparse[entity.index];
        if (dense_idx != SENTINEL) {
            auto& slot = m_dense[dense_idx];
            if (slot.id.generation == entity.generation) {
                // Existing component — overwrite data only.
                slot.data = T{};
                return slot.data;
            }
            // Stale sparse entry (generation mismatch from entity
            // destroyed without component removal) — reclaim the
            // dense slot by updating the stored entity ID.
            slot.id = entity;
            slot.data = T{};
            return slot.data;
        }

        // New entry.
        dense_idx = static_cast<u32>(m_dense.size());
        m_dense.push_back({entity, T{}});
        m_sparse[entity.index] = dense_idx;
        ++m_count;
        return m_dense.back().data;
    }

    // Remove component from entity.
    void remove(EntityId entity) {
        if (entity.index >= m_sparse.size()) return;
        u32 dense_idx = m_sparse[entity.index];
        if (dense_idx == SENTINEL) return;
        // Stale generation or dead registry entry is still a valid
        // removal — clean up the dense slot regardless.
        if (m_dense[dense_idx].id.generation != entity.generation) return;
        remove_at(dense_idx, entity.index);
    }

    void clear() {
        m_dense.clear();
        m_sparse.clear();
        m_count = 0;
    }

    u32 count() const { return m_count; }

    // Iterate all alive components. Callback receives (EntityId, T&).
    // Dense iteration is cache-friendly — no empty slots visited.
    template <typename F>
    void each(F&& func) {
        for (u32 i = 0; i < static_cast<u32>(m_dense.size()); ++i) {
            func(m_dense[i].id, m_dense[i].data);
        }
    }

    template <typename F>
    void each(F&& func) const {
        for (u32 i = 0; i < static_cast<u32>(m_dense.size()); ++i) {
            func(m_dense[i].id, m_dense[i].data);
        }
    }

private:
    // Remove the component at the given dense index (swap-with-last).
    void remove_at(u32 dense_idx, u32 entity_idx) {
        u32 last_idx = static_cast<u32>(m_dense.size()) - 1;
        if (dense_idx != last_idx) {
            auto& last_slot = m_dense[last_idx];
            m_dense[dense_idx] = last_slot;
            m_sparse[last_slot.id.index] = dense_idx;
        }
        m_dense.pop_back();
        m_sparse[entity_idx] = SENTINEL;
        --m_count;
    }

    struct DenseSlot {
        EntityId id;
        T data;
    };

    std::vector<DenseSlot> m_dense;
    std::vector<u32>       m_sparse;  // entity.index → dense index (or SENTINEL)
    u32                    m_count = 0;
    const EntityRegistry*  m_registry = nullptr;
};

} // namespace pino
