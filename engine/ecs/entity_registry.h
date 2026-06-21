#pragma once

#include "engine/ecs/entity.h"
#include <vector>

namespace pino {

// Owns all entities in a packed slot array with generation-based ID reuse.
// Entity identity is separate from any component data — this is only the
// identity layer. Future ECS systems can add component storage on top.
class EntityRegistry {
public:
    EntityRegistry();
    ~EntityRegistry() = default;

    EntityRegistry(const EntityRegistry&) = delete;
    EntityRegistry& operator=(const EntityRegistry&) = delete;

    // Create a new entity. Returns a stable ID that remains valid until
    // destroy() is called (the slot is recycled with a bumped generation).
    EntityId create();

    // Destroy an entity. Its slot becomes reusable; any existing
    // EntityHandle pointing to it will read as stale.
    void destroy(EntityId id);

    // Returns true if id refers to a currently alive entity.
    bool alive(EntityId id) const;

    // Number of alive entities.
    u32 count() const { return m_count; }

    // Total allocated slots (alive + free).
    u32 capacity() const { return static_cast<u32>(m_slots.size()); }

    // Destroy all entities.
    void clear();

    // Iterate all alive entities. The callback receives each EntityId.
    template <typename F>
    void each(F&& func) const {
        for (u32 i = 0; i < static_cast<u32>(m_slots.size()); ++i) {
            if (m_slots[i].alive) {
                EntityId id{i, m_slots[i].generation};
                func(id);
            }
        }
    }

    template <typename F>
    void each(F&& func) {
        for (u32 i = 0; i < static_cast<u32>(m_slots.size()); ++i) {
            if (m_slots[i].alive) {
                EntityId id{i, m_slots[i].generation};
                func(id);
            }
        }
    }

private:
    struct Slot {
        u32 generation = 0;
        bool alive     = false;
    };

    std::vector<Slot> m_slots;
    std::vector<u32>  m_free_list;  // indices of freed slots for reuse
    u32 m_count = 0;
};

inline EntityRegistry::EntityRegistry() {
    m_slots.reserve(64);
    m_free_list.reserve(64);
}

inline EntityId EntityRegistry::create() {
    u32 idx;
    if (!m_free_list.empty()) {
        idx = m_free_list.back();
        m_free_list.pop_back();
    } else {
        idx = static_cast<u32>(m_slots.size());
        m_slots.push_back({});
    }
    auto& slot = m_slots[idx];
    slot.alive = true;
    // generation stays as-is from previous allocation
    ++m_count;
    return EntityId{idx, slot.generation};
}

inline void EntityRegistry::destroy(EntityId id) {
    if (!alive(id)) return;
    auto& slot = m_slots[id.index];
    slot.alive = false;
    ++slot.generation;   // invalidate stale handles
    m_free_list.push_back(id.index);
    --m_count;
}

inline bool EntityRegistry::alive(EntityId id) const {
    if (id.index >= static_cast<u32>(m_slots.size())) return false;
    const auto& slot = m_slots[id.index];
    return slot.alive && slot.generation == id.generation;
}

inline void EntityRegistry::clear() {
    m_slots.clear();
    m_free_list.clear();
    m_count = 0;
}

} // namespace pino
