#include "engine/input/gamepad.h"
#include "engine/core/log.h"
#include <cmath>
#include <cstring>

#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
#include <SDL.h>
#endif

namespace pino {

GamepadManager::GamepadManager() {
    for (auto& c : m_controllers) std::memset(&c, 0, sizeof(c));
}

GamepadManager::~GamepadManager() {
#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
    for (i32 i = 0; i < MAX_GAMEPADS; ++i) {
        if (m_controllers[i].sdl_controller) {
            SDL_GameControllerClose(
                static_cast<SDL_GameController*>(m_controllers[i].sdl_controller));
        }
    }
#endif
}

void GamepadManager::on_controller_added(i32 device_index) {
#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
    i32 slot = find_free_slot();
    if (slot < 0) {
        PINO_WARN("Too many gamepads, ignoring device %d", device_index);
        return;
    }
    auto& c = m_controllers[slot];
    c.sdl_controller = SDL_GameControllerOpen(device_index);
    if (!c.sdl_controller) {
        PINO_WARN("Failed to open gamepad %d: %s", device_index, SDL_GetError());
        return;
    }
    c.instance_id = SDL_JoystickInstanceID(
        SDL_GameControllerGetJoystick(static_cast<SDL_GameController*>(c.sdl_controller)));
    c.attached = true;
    std::memset(&c.state, 0, sizeof(c.state));
    std::memset(&c.prev_state, 0, sizeof(c.prev_state));
    ++m_count;
    PINO_INFO("Gamepad connected: slot=%d instance=%d", slot, c.instance_id);
#else
    (void)device_index;
#endif
}

void GamepadManager::on_controller_removed(i32 instance_id) {
    i32 slot = find_by_instance(instance_id);
    if (slot < 0) return;
    auto& c = m_controllers[slot];
#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
    if (c.sdl_controller)
        SDL_GameControllerClose(static_cast<SDL_GameController*>(c.sdl_controller));
#endif
    std::memset(&c, 0, sizeof(Controller));
    --m_count;
    PINO_INFO("Gamepad disconnected: instance=%d", instance_id);
}

void GamepadManager::on_axis_motion(i32 instance_id, i32 axis, i16 value) {
    i32 slot = find_by_instance(instance_id);
    if (slot < 0) return;

    float val = static_cast<float>(value) / 32767.0f;
    if (val > 1.0f) val = 1.0f;
    if (val < -1.0f) val = -1.0f;

    // Apply deadzone
    auto apply_dz = [&](float v) -> float {
        if (std::fabs(v) < m_deadzone) return 0.0f;
        return (v > 0 ? 1.0f : -1.0f) * (std::fabs(v) - m_deadzone) / (1.0f - m_deadzone);
    };

    auto& s = m_controllers[slot].state;
    switch (axis) {
        case 0: s.left_stick_x  = apply_dz(val); break; // SDL_CONTROLLER_AXIS_LEFTX
        case 1: s.left_stick_y  = apply_dz(val); break; // SDL_CONTROLLER_AXIS_LEFTY
        case 2: s.right_stick_x = apply_dz(val); break; // SDL_CONTROLLER_AXIS_RIGHTX
        case 3: s.right_stick_y = apply_dz(val); break; // SDL_CONTROLLER_AXIS_RIGHTY
        case 4: s.left_trigger  = (val + 1.0f) * 0.5f; break; // SDL_CONTROLLER_AXIS_TRIGGERLEFT
        case 5: s.right_trigger = (val + 1.0f) * 0.5f; break; // SDL_CONTROLLER_AXIS_TRIGGERRIGHT
        default: break;
    }
}

void GamepadManager::on_button(i32 instance_id, i32 button, bool pressed) {
    i32 slot = find_by_instance(instance_id);
    if (slot < 0) return;
    auto& s = m_controllers[slot].state;
    switch (button) {
        case 0:  s.a = pressed; break; // SDL_CONTROLLER_BUTTON_A
        case 1:  s.b = pressed; break;
        case 2:  s.x = pressed; break;
        case 3:  s.y = pressed; break;
        case 4:  s.back = pressed; break;
        case 5:  s.guide = pressed; break;
        case 6:  s.start = pressed; break;
        case 7:  s.left_stick = pressed; break;
        case 8:  s.right_stick = pressed; break;
        case 9:  s.left_shoulder = pressed; break;
        case 10: s.right_shoulder = pressed; break;
        case 11: s.dpad_up = pressed; break;
        case 12: s.dpad_down = pressed; break;
        case 13: s.dpad_left = pressed; break;
        case 14: s.dpad_right = pressed; break;
        default: break;
    }
}

void GamepadManager::frame_advance() {
    for (i32 i = 0; i < MAX_GAMEPADS; ++i) {
        if (m_controllers[i].attached)
            m_controllers[i].prev_state = m_controllers[i].state;
    }
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

void GamepadManager::set_rumble(GamepadHandle h, float low_freq, float high_freq, u32 duration_ms) {
#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
    if (!h.is_valid() || h.index >= MAX_GAMEPADS) return;
    auto& c = m_controllers[h.index];
    if (!c.sdl_controller) return;
    SDL_GameControllerRumble(
        static_cast<SDL_GameController*>(c.sdl_controller),
        static_cast<u16>(low_freq * 0xFFFF),
        static_cast<u16>(high_freq * 0xFFFF),
        duration_ms);
#else
    (void)h; (void)low_freq; (void)high_freq; (void)duration_ms;
#endif
}

i32 GamepadManager::find_by_instance(i32 id) const {
    for (i32 i = 0; i < MAX_GAMEPADS; ++i)
        if (m_controllers[i].attached && m_controllers[i].instance_id == id)
            return i;
    return -1;
}

i32 GamepadManager::find_free_slot() const {
    for (i32 i = 0; i < MAX_GAMEPADS; ++i)
        if (!m_controllers[i].attached)
            return i;
    return -1;
}

} // namespace pino
