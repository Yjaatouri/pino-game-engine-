#pragma once

#include "engine/core/types.h"
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <vector>
#include <memory>

namespace pino {

// ── Event types ─────────────────────────────────────────────────
struct CollisionEnterEvent {
    class Entity* a = nullptr;
    class Entity* b = nullptr;
};

struct CollisionStayEvent {
    class Entity* a = nullptr;
    class Entity* b = nullptr;
};

struct CollisionExitEvent {
    class Entity* a = nullptr;
    class Entity* b = nullptr;
};

// Backward-compatible alias
using CollisionEvent = CollisionStayEvent;

struct InputEvent {
    i32 key = 0;
    i32 action = 0; // 0=press, 1=release, 2=repeat
};

struct SceneLoadedEvent {
    class Scene* scene = nullptr;
};

struct EntityDestroyedEvent {
    class Entity* entity = nullptr;
};

// ── EventBus ────────────────────────────────────────────────────
class EventBus {
public:
    using HandlerId = u32;

    EventBus() = default;
    ~EventBus() = default;

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    // Subscribe to an event type. Returns a handle that can be used to unsubscribe.
    template <typename T>
    HandlerId subscribe(std::function<void(const T&)> callback) {
        auto& handlers = get_handlers<T>();
        HandlerId id = m_next_id++;
        handlers.push_back({id, [cb = std::move(callback)](const void* data) {
            cb(*static_cast<const T*>(data));
        }});
        return id;
    }

    // Unsubscribe by handler id (safe: silenty ignores unknown ids)
    void unsubscribe(HandlerId id) {
        if (id == 0) return; // 0 is invalid
        for (auto& [type, vec] : m_handlers) {
            auto it = std::find_if(vec.begin(), vec.end(),
                                   [id](const HandlerEntry& e) { return e.id == id; });
            if (it != vec.end()) {
                vec.erase(it);
                return;
            }
        }
    }

    // Emit an event to all subscribers
    template <typename T>
    void emit(const T& event) {
        auto it = m_handlers.find(std::type_index(typeid(T)));
        if (it == m_handlers.end()) return;
        // Copy the list so subscribers can unsubscribe during dispatch
        auto handlers_copy = it->second;
        for (auto& entry : handlers_copy) {
            entry.fn(&event);
        }
    }

    // Remove all handlers for a specific event type
    template <typename T>
    void clear_handlers() {
        m_handlers.erase(std::type_index(typeid(T)));
    }

    // Remove all handlers
    void clear_all() {
        m_handlers.clear();
    }

    static EventBus& instance() {
        static EventBus bus;
        return bus;
    }

private:
    struct HandlerEntry {
        HandlerId id;
        std::function<void(const void*)> fn;
    };

    template <typename T>
    std::vector<HandlerEntry>& get_handlers() {
        return m_handlers[std::type_index(typeid(T))];
    }

    std::unordered_map<std::type_index, std::vector<HandlerEntry>> m_handlers;
    HandlerId m_next_id = 1;
};

} // namespace pino
