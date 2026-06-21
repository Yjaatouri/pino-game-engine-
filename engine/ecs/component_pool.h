#pragma once

#include "engine/ecs/entity.h"
#include "engine/ecs/entity_registry.h"
#include <vector>

namespace pino {

// Sparse component storage indexed by entity index.
// Cross-checks with EntityRegistry to detect stale IDs from
// destroyed entities, even when the slot has not been reused.
//
// If set_registry() is called, get()/has() also verify that the
// entity is alive in the registry, preventing false positives
// after destruction.
template <typename T>
class ComponentPool {
public:
    ComponentPool() { m_slots.reserve(64); }

    // Optional registry for cross-checking entity liveness.
    void set_registry(const EntityRegistry* reg) { m_registry = reg; }

    // Returns pointer to component if entity has one, else nullptr.
    T* get(EntityId entity) {
        if (entity.index >= m_slots.size()) return nullptr;
        auto& slot = m_slots[entity.index];
        if (!slot.exists) return nullptr;
        if (slot.generation != entity.generation) return nullptr;
        if (m_registry && !m_registry->alive(entity)) return nullptr;
        return &slot.data;
    }

    const T* get(EntityId entity) const {
        if (entity.index >= m_slots.size()) return nullptr;
        const auto& slot = m_slots[entity.index];
        if (!slot.exists) return nullptr;
        if (slot.generation != entity.generation) return nullptr;
        if (m_registry && !m_registry->alive(entity)) return nullptr;
        return &slot.data;
    }

    bool has(EntityId entity) const {
        return get(entity) != nullptr;
    }

    // Add a component (default-constructed). Returns reference.
    T& add(EntityId entity) {
        if (entity.index >= m_slots.size())
            m_slots.resize(entity.index + 1);
        auto& slot = m_slots[entity.index];
        slot.data = T{};
        slot.generation = entity.generation;
        slot.exists = true;
        ++m_count;
        return slot.data;
    }

    // Remove component from entity.
    void remove(EntityId entity) {
        auto* ptr = get(entity);
        if (!ptr) return;
        m_slots[entity.index].exists = false;
        --m_count;
    }

    void clear() {
        m_slots.clear();
        m_count = 0;
    }

    u32 count() const { return m_count; }

    // Iterate all alive components. Callback receives (EntityId, T&).
    template <typename F>
    void each(F&& func) {
        for (u32 i = 0; i < m_slots.size(); ++i) {
            auto& slot = m_slots[i];
            if (slot.exists) {
                EntityId id{i, slot.generation};
                func(id, slot.data);
            }
        }
    }

    template <typename F>
    void each(F&& func) const {
        for (u32 i = 0; i < m_slots.size(); ++i) {
            const auto& slot = m_slots[i];
            if (slot.exists) {
                EntityId id{i, slot.generation};
                func(id, slot.data);
            }
        }
    }

private:
    struct Slot {
        T data{};
        u32 generation = 0;
        bool exists = false;
    };

    std::vector<Slot> m_slots;
    u32 m_count = 0;
    const EntityRegistry* m_registry = nullptr;
};

} // namespace pino
