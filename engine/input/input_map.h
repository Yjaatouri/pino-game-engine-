#pragma once

#include "engine/core/types.h"
#include "engine/platform/input.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace pino {

// -----------------------------------------------------------------------
// Input Context — stack-based priority layer for action filtering
// -----------------------------------------------------------------------
enum class InputContext : u8 {
    Gameplay = 0,
    UI       = 1,
    Debug    = 2,
    COUNT    = 3,
};

// -----------------------------------------------------------------------
// InputMap — action-to-input binding with context support
// -----------------------------------------------------------------------
class InputMap {
public:
    InputMap() = default;

    // ---- Binding ----
    // Bind a keyboard key to an action in a specific context.
    void bind_key(const std::string& action, Key key, InputContext ctx = InputContext::Gameplay) {
        m_bindings[static_cast<usize>(ctx)].key_bindings[action].push_back(key);
    }

    // Unbind a specific key from an action in a context.
    void unbind_key(const std::string& action, Key key, InputContext ctx = InputContext::Gameplay) {
        auto& vec = m_bindings[static_cast<usize>(ctx)].key_bindings[action];
        vec.erase(std::remove(vec.begin(), vec.end(), key), vec.end());
    }

    // Unbind all keys from an action in a context.
    void unbind_action(const std::string& action, InputContext ctx = InputContext::Gameplay) {
        m_bindings[static_cast<usize>(ctx)].key_bindings.erase(action);
        m_bindings[static_cast<usize>(ctx)].button_bindings.erase(action);
    }

    // Remove all bindings for a context.
    void clear_context(InputContext ctx) {
        m_bindings[static_cast<usize>(ctx)].key_bindings.clear();
        m_bindings[static_cast<usize>(ctx)].button_bindings.clear();
    }

    // Remove all bindings.
    void clear_all() {
        for (auto& ctx : m_bindings) {
            ctx.key_bindings.clear();
            ctx.button_bindings.clear();
        }
    }

    // ---- Context stack management ----
    void push_context(InputContext ctx) {
        if (m_context_depth < MAX_CONTEXT_DEPTH)
            m_context_stack[m_context_depth++] = ctx;
    }

    void pop_context() {
        if (m_context_depth > 0) --m_context_depth;
    }

    void set_context(InputContext ctx) {
        m_context_depth = 1;
        m_context_stack[0] = ctx;
    }

    InputContext active_context() const {
        return m_context_depth > 0 ? m_context_stack[m_context_depth - 1] : InputContext::Gameplay;
    }

    u32 context_depth() const { return static_cast<u32>(m_context_depth); }

    // ---- Action queries (walk context stack top-down) ----
    bool is_action_pressed(const std::string& action) const {
        auto* in = Input::instance();
        if (!in) return false;
        for (i32 d = m_context_depth - 1; d >= 0; --d) {
            auto ctx = m_context_stack[d];
            // Check key bindings
            auto kit = m_bindings[static_cast<usize>(ctx)].key_bindings.find(action);
            if (kit != m_bindings[static_cast<usize>(ctx)].key_bindings.end()) {
                for (auto k : kit->second)
                    if (in->is_key_pressed(k)) return true;
            }
            // Check button bindings (gamepad)
            auto bit = m_bindings[static_cast<usize>(ctx)].button_bindings.find(action);
            if (bit != m_bindings[static_cast<usize>(ctx)].button_bindings.end()) {
                for (auto b : bit->second)
                    if (is_gamepad_button_pressed(b)) return true;
            }
        }
        return false;
    }

    bool is_action_just_pressed(const std::string& action) const {
        auto* in = Input::instance();
        if (!in) return false;
        for (i32 d = m_context_depth - 1; d >= 0; --d) {
            auto ctx = m_context_stack[d];
            auto kit = m_bindings[static_cast<usize>(ctx)].key_bindings.find(action);
            if (kit != m_bindings[static_cast<usize>(ctx)].key_bindings.end()) {
                for (auto k : kit->second)
                    if (in->is_key_just_pressed(k)) return true;
            }
            auto bit = m_bindings[static_cast<usize>(ctx)].button_bindings.find(action);
            if (bit != m_bindings[static_cast<usize>(ctx)].button_bindings.end()) {
                for (auto b : bit->second)
                    if (is_gamepad_button_just_pressed(b)) return true;
            }
        }
        return false;
    }

    bool is_action_released(const std::string& action) const {
        auto* in = Input::instance();
        if (!in) return false;
        for (i32 d = m_context_depth - 1; d >= 0; --d) {
            auto ctx = m_context_stack[d];
            auto kit = m_bindings[static_cast<usize>(ctx)].key_bindings.find(action);
            if (kit != m_bindings[static_cast<usize>(ctx)].key_bindings.end()) {
                for (auto k : kit->second)
                    if (in->is_key_just_released(k)) return true;
            }
            auto bit = m_bindings[static_cast<usize>(ctx)].button_bindings.find(action);
            if (bit != m_bindings[static_cast<usize>(ctx)].button_bindings.end()) {
                for (auto b : bit->second)
                    if (is_gamepad_button_just_released(b)) return true;
            }
        }
        return false;
    }

    // ---- Gamepad binding (future use; button constants defined externally) ----
    using GamepadButtonCode = u8;
    void bind_button(const std::string& action, GamepadButtonCode button, InputContext ctx = InputContext::Gameplay) {
        m_bindings[static_cast<usize>(ctx)].button_bindings[action].push_back(button);
    }

private:
    static constexpr u32 MAX_CONTEXT_DEPTH = 8;

    struct ContextBindings {
        std::unordered_map<std::string, std::vector<Key>>              key_bindings;
        std::unordered_map<std::string, std::vector<GamepadButtonCode>> button_bindings;
    };

    ContextBindings m_bindings[static_cast<usize>(InputContext::COUNT)];
    InputContext    m_context_stack[MAX_CONTEXT_DEPTH]{};
    u32             m_context_depth = 1; // defaults to Gameplay

    bool is_gamepad_button_pressed(GamepadButtonCode) const {
        return false; // TODO: integrate with GamepadManager
    }
    bool is_gamepad_button_just_pressed(GamepadButtonCode) const {
        return false;
    }
    bool is_gamepad_button_just_released(GamepadButtonCode) const {
        return false;
    }
};

} // namespace pino
