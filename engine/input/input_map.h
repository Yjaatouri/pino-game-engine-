#pragma once

#include "engine/core/types.h"
#include "engine/platform/input.h"
#include "engine/input/gamepad.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace pino {

// Input Context — stack-based priority layer for action filtering
enum class InputContext : u8 {
    Gameplay = 0,
    UI       = 1,
    Debug    = 2,
    COUNT    = 3,
};

// InputMap — action-to-input binding with context support.
// Queries all bound devices (keyboard, mouse, gamepad) for each action.
class InputMap {
public:
    InputMap() = default;

    // Provide a GamepadManager (set once after construction during engine init).
    void set_gamepad_manager(GamepadManager* mgr) { m_gamepad_mgr = mgr; }

    // ---- Binding ----

    void bind_key(const std::string& action, Key key, InputContext ctx = InputContext::Gameplay) {
        m_bindings[static_cast<usize>(ctx)].key_bindings[action].push_back(key);
    }

    void bind_mouse_button(const std::string& action, MouseButton button, InputContext ctx = InputContext::Gameplay) {
        m_bindings[static_cast<usize>(ctx)].mouse_bindings[action].push_back(button);
    }

    void bind_gamepad_button(const std::string& action, GamepadButton button, InputContext ctx = InputContext::Gameplay) {
        m_bindings[static_cast<usize>(ctx)].gamepad_bindings[action].push_back(button);
    }

    void unbind_key(const std::string& action, Key key, InputContext ctx = InputContext::Gameplay) {
        auto& vec = m_bindings[static_cast<usize>(ctx)].key_bindings[action];
        vec.erase(std::remove(vec.begin(), vec.end(), key), vec.end());
    }

    void unbind_mouse_button(const std::string& action, MouseButton button, InputContext ctx = InputContext::Gameplay) {
        auto& vec = m_bindings[static_cast<usize>(ctx)].mouse_bindings[action];
        vec.erase(std::remove(vec.begin(), vec.end(), button), vec.end());
    }

    void unbind_gamepad_button(const std::string& action, GamepadButton button, InputContext ctx = InputContext::Gameplay) {
        auto& vec = m_bindings[static_cast<usize>(ctx)].gamepad_bindings[action];
        vec.erase(std::remove(vec.begin(), vec.end(), button), vec.end());
    }

    void unbind_action(const std::string& action, InputContext ctx = InputContext::Gameplay) {
        m_bindings[static_cast<usize>(ctx)].key_bindings.erase(action);
        m_bindings[static_cast<usize>(ctx)].mouse_bindings.erase(action);
        m_bindings[static_cast<usize>(ctx)].gamepad_bindings.erase(action);
    }

    void clear_context(InputContext ctx) {
        m_bindings[static_cast<usize>(ctx)].key_bindings.clear();
        m_bindings[static_cast<usize>(ctx)].mouse_bindings.clear();
        m_bindings[static_cast<usize>(ctx)].gamepad_bindings.clear();
    }

    void clear_all() {
        for (auto& ctx : m_bindings) {
            ctx.key_bindings.clear();
            ctx.mouse_bindings.clear();
            ctx.gamepad_bindings.clear();
        }
    }

    // ---- Context stack ----
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
    // Returns true if any bound input for the action is active this frame.
    bool action_pressed(const std::string& action) const {
        auto* in = Input::instance();
        if (!in) return false;
        for (i32 d = m_context_depth - 1; d >= 0; --d) {
            auto ctx = m_context_stack[d];
            auto& bindings = m_bindings[static_cast<usize>(ctx)];

            // Keyboard
            auto kit = bindings.key_bindings.find(action);
            if (kit != bindings.key_bindings.end())
                for (auto k : kit->second)
                    if (in->is_key_pressed(k)) return true;

            // Mouse
            auto mit = bindings.mouse_bindings.find(action);
            if (mit != bindings.mouse_bindings.end())
                for (auto b : mit->second)
                    if (in->is_mouse_pressed(b)) return true;

            // Gamepad (first connected)
            auto git = bindings.gamepad_bindings.find(action);
            if (git != bindings.gamepad_bindings.end() && m_gamepad_mgr) {
                auto* gs = m_gamepad_mgr->get_state(0);
                if (gs)
                    for (auto b : git->second)
                        if (gs->is_down(b)) return true;
            }
        }
        return false;
    }

    bool action_just_pressed(const std::string& action) const {
        auto* in = Input::instance();
        if (!in) return false;
        for (i32 d = m_context_depth - 1; d >= 0; --d) {
            auto ctx = m_context_stack[d];
            auto& bindings = m_bindings[static_cast<usize>(ctx)];

            auto kit = bindings.key_bindings.find(action);
            if (kit != bindings.key_bindings.end())
                for (auto k : kit->second)
                    if (in->is_key_just_pressed(k)) return true;

            auto mit = bindings.mouse_bindings.find(action);
            if (mit != bindings.mouse_bindings.end())
                for (auto b : mit->second)
                    if (in->is_mouse_just_pressed(b)) return true;

            auto git = bindings.gamepad_bindings.find(action);
            if (git != bindings.gamepad_bindings.end() && m_gamepad_mgr) {
                auto* gs = m_gamepad_mgr->get_state(0);
                if (gs)
                    for (auto b : git->second)
                        if (gs->just_pressed(b)) return true;
            }
        }
        return false;
    }

    bool action_released(const std::string& action) const {
        auto* in = Input::instance();
        if (!in) return false;
        for (i32 d = m_context_depth - 1; d >= 0; --d) {
            auto ctx = m_context_stack[d];
            auto& bindings = m_bindings[static_cast<usize>(ctx)];

            auto kit = bindings.key_bindings.find(action);
            if (kit != bindings.key_bindings.end())
                for (auto k : kit->second)
                    if (in->is_key_just_released(k)) return true;

            auto mit = bindings.mouse_bindings.find(action);
            if (mit != bindings.mouse_bindings.end())
                for (auto b : mit->second)
                    if (in->is_mouse_just_released(b)) return true;

            auto git = bindings.gamepad_bindings.find(action);
            if (git != bindings.gamepad_bindings.end() && m_gamepad_mgr) {
                auto* gs = m_gamepad_mgr->get_state(0);
                if (gs)
                    for (auto b : git->second)
                        if (gs->just_released(b)) return true;
            }
        }
        return false;
    }

    // ---- Legacy convenience wrappers (backward compat) ----
    bool is_action_pressed(const std::string& action) const { return action_pressed(action); }
    bool is_action_just_pressed(const std::string& action) const { return action_just_pressed(action); }
    bool is_action_released(const std::string& action) const { return action_released(action); }

private:
    static constexpr u32 MAX_CONTEXT_DEPTH = 8;

    struct ContextBindings {
        std::unordered_map<std::string, std::vector<Key>>          key_bindings;
        std::unordered_map<std::string, std::vector<MouseButton>>  mouse_bindings;
        std::unordered_map<std::string, std::vector<GamepadButton>> gamepad_bindings;
    };

    ContextBindings m_bindings[static_cast<usize>(InputContext::COUNT)];
    InputContext    m_context_stack[MAX_CONTEXT_DEPTH]{};
    u32             m_context_depth = 1;
    GamepadManager* m_gamepad_mgr = nullptr;
};

} // namespace pino
