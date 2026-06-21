#include "engine/input/gamepad.h"
#include <cmath>

namespace pino {

GamepadManager::GamepadManager() {
    for (auto& c : m_controllers)
        std::memset(&c, 0, sizeof(c));
}

void GamepadManager::begin_frame() {
    for (i32 i = 0; i < MAX_GAMEPADS; ++i) {
        if (m_controllers[i].attached)
            m_controllers[i].state.buttons_prev = m_controllers[i].state.buttons;
    }
}

void GamepadManager::on_connect(GamepadHandle h) {
    if (!h.is_valid() || h.index >= MAX_GAMEPADS) return;
    auto& c = m_controllers[h.index];
    if (c.attached) return;
    std::memset(&c.state, 0, sizeof(c.state));
    c.attached = true;
    ++m_count;
}

void GamepadManager::on_disconnect(GamepadHandle h) {
    if (!h.is_valid() || h.index >= MAX_GAMEPADS) return;
    auto& c = m_controllers[h.index];
    if (!c.attached) return;
    std::memset(&c, 0, sizeof(Controller));
    --m_count;
}

void GamepadManager::set_button(GamepadHandle h, GamepadButton b, bool pressed) {
    if (!h.is_valid() || h.index >= MAX_GAMEPADS) return;
    auto& s = m_controllers[h.index].state;
    u16 mask = 1u << static_cast<u8>(b);
    if (pressed)
        s.buttons |= mask;
    else
        s.buttons &= ~mask;
}

void GamepadManager::set_axis(GamepadHandle h, GamepadAxis a, float value) {
    if (!h.is_valid() || h.index >= MAX_GAMEPADS) return;
    if (static_cast<usize>(a) >= static_cast<usize>(GamepadAxis::COUNT)) return;
    m_controllers[h.index].state.axes[static_cast<usize>(a)] = value;
}

GamepadHandle GamepadManager::handle(i32 index) const {
    i32 count = 0;
    for (i32 i = 0; i < MAX_GAMEPADS; ++i) {
        if (m_controllers[i].attached) {
            if (count == index) return GamepadHandle{i};
            ++count;
        }
    }
    return GamepadHandle{};
}

const GamepadState* GamepadManager::get_state(GamepadHandle h) const {
    if (!h.is_valid() || h.index >= MAX_GAMEPADS) return nullptr;
    if (!m_controllers[h.index].attached) return nullptr;
    return &m_controllers[h.index].state;
}

const GamepadState* GamepadManager::get_state(i32 index) const {
    auto h = handle(index);
    return get_state(h);
}

void GamepadManager::set_rumble(GamepadHandle h, float low, float high, u32 duration_ms) {
    if (m_rumble_fn && h.is_valid())
        m_rumble_fn(h.index, low, high, duration_ms);
}

float GamepadManager::apply_deadzone(float value) const {
    if (std::fabs(value) < m_deadzone) return 0.0f;
    return (value > 0 ? 1.0f : -1.0f) * (std::fabs(value) - m_deadzone) / (1.0f - m_deadzone);
}

void GamepadManager::reset_state() {
    for (i32 i = 0; i < MAX_GAMEPADS; ++i) {
        if (m_controllers[i].attached)
            std::memset(&m_controllers[i].state, 0, sizeof(GamepadState));
    }
}

} // namespace pino
