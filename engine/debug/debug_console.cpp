#include "engine/debug/debug_console.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

namespace pino {

// ─── Key-to-character mapping helpers ─────────────────────────────

static char key_to_char(Key k, bool shift) {
    char c = 0;
         if (k >= Key::A && k <= Key::Z) c = static_cast<char>('a' + (static_cast<int>(k) - static_cast<int>(Key::A)));
    else if (k >= Key::_0 && k <= Key::_9) c = static_cast<char>('0' + (static_cast<int>(k) - static_cast<int>(Key::_0)));
    else if (k >= Key::KP_0 && k <= Key::KP_9) c = static_cast<char>('0' + (static_cast<int>(k) - static_cast<int>(Key::KP_0)));
    else {
        switch (k) {
            case Key::Space:     c = ' '; break;
            case Key::Minus:     c = '-'; break;
            case Key::Equals:    c = '='; break;
            case Key::LBracket:  c = '['; break;
            case Key::RBracket:  c = ']'; break;
            case Key::Semicolon: c = ';'; break;
            case Key::Quote:     c = '\''; break;
            case Key::Comma:     c = ','; break;
            case Key::Period:    c = '.'; break;
            case Key::Slash:     c = '/'; break;
            case Key::Backslash: c = '\\'; break;
            case Key::Grave:     c = '`'; break;
            case Key::KP_Decimal: c = '.'; break;
            case Key::KP_Divide:  c = '/'; break;
            case Key::KP_Multiply:c = '*'; break;
            case Key::KP_Subtract:c = '-'; break;
            case Key::KP_Add:     c = '+'; break;
            default: break;
        }
    }

    if (c != 0 && shift) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        } else {
            switch (c) {
                case '1': c = '!'; break; case '2': c = '@'; break;
                case '3': c = '#'; break; case '4': c = '$'; break;
                case '5': c = '%'; break; case '6': c = '^'; break;
                case '7': c = '&'; break; case '8': c = '*'; break;
                case '9': c = '('; break; case '0': c = ')'; break;
                case '-': c = '_'; break; case '=': c = '+'; break;
                case '[': c = '{'; break; case ']': c = '}'; break;
                case ';': c = ':'; break; case '\'': c = '"'; break;
                case ',': c = '<'; break; case '.': c = '>'; break;
                case '/': c = '?'; break; case '\\': c = '|'; break;
                case '`': c = '~'; break;
                default: break;
            }
        }
    }
    return c;
}

// ─── Printable keys to poll ───────────────────────────────────────

static const Key s_printable_keys[] = {
    Key::A, Key::B, Key::C, Key::D, Key::E, Key::F, Key::G,
    Key::H, Key::I, Key::J, Key::K, Key::L, Key::M,
    Key::N, Key::O, Key::P, Key::Q, Key::R, Key::S,
    Key::T, Key::U, Key::V, Key::W, Key::X, Key::Y, Key::Z,
    Key::_0, Key::_1, Key::_2, Key::_3, Key::_4,
    Key::_5, Key::_6, Key::_7, Key::_8, Key::_9,
    Key::Space,
    Key::Minus, Key::Equals,
    Key::LBracket, Key::RBracket,
    Key::Semicolon, Key::Quote,
    Key::Comma, Key::Period, Key::Slash,
    Key::Backslash, Key::Grave,
    Key::KP_0, Key::KP_1, Key::KP_2, Key::KP_3, Key::KP_4,
    Key::KP_5, Key::KP_6, Key::KP_7, Key::KP_8, Key::KP_9,
    Key::KP_Decimal, Key::KP_Divide, Key::KP_Multiply,
    Key::KP_Subtract, Key::KP_Add,
};

// ─── Constructor / Destructor ─────────────────────────────────────

DebugConsole::DebugConsole() = default;
DebugConsole::~DebugConsole() = default;

void DebugConsole::toggle() {
    m_visible = !m_visible;
    if (m_visible) {
        scroll_to_bottom();
        m_input.clear();
        m_cursor_pos = 0;
    }
}

// ─── Input handling ───────────────────────────────────────────────

bool DebugConsole::handle_input(Input& input) {
    // Toggle always works
    if (input.is_key_just_pressed(Key::F10)) {
        toggle();
        return true;
    }
    if (!m_visible) return false;

    bool shift = input.is_key_pressed(Key::LShift) || input.is_key_pressed(Key::RShift);

    // Printable characters
    for (Key k : s_printable_keys) {
        if (input.is_key_just_pressed(k)) {
            char c = key_to_char(k, shift);
            if (c) {
                m_input.insert(m_cursor_pos, 1, c);
                ++m_cursor_pos;
            }
        }
    }

    // Editing
    if (input.is_key_just_pressed(Key::Backspace)) {
        if (m_cursor_pos > 0 && !m_input.empty()) {
            m_input.erase(m_cursor_pos - 1, 1);
            --m_cursor_pos;
        }
    }
    if (input.is_key_just_pressed(Key::Delete)) {
        if (m_cursor_pos < static_cast<i32>(m_input.size())) {
            m_input.erase(m_cursor_pos, 1);
        }
    }

    // Cursor navigation
    if (input.is_key_just_pressed(Key::Left)) {
        if (m_cursor_pos > 0) --m_cursor_pos;
    }
    if (input.is_key_just_pressed(Key::Right)) {
        if (m_cursor_pos < static_cast<i32>(m_input.size())) ++m_cursor_pos;
    }
    if (input.is_key_just_pressed(Key::Home)) {
        m_cursor_pos = 0;
    }
    if (input.is_key_just_pressed(Key::End)) {
        m_cursor_pos = static_cast<i32>(m_input.size());
    }

    // History cycling
    if (input.is_key_just_pressed(Key::Up)) {
        if (!m_history.empty()) {
            if (m_history_idx < 0) {
                m_history_idx = static_cast<i32>(m_history.size()) - 1;
            } else if (m_history_idx > 0) {
                --m_history_idx;
            }
            m_input = m_history[m_history_idx];
            m_cursor_pos = static_cast<i32>(m_input.size());
        }
    }
    if (input.is_key_just_pressed(Key::Down)) {
        if (m_history_idx >= 0) {
            ++m_history_idx;
            if (m_history_idx >= static_cast<i32>(m_history.size())) {
                m_history_idx = -1;
                m_input.clear();
            } else {
                m_input = m_history[m_history_idx];
            }
            m_cursor_pos = static_cast<i32>(m_input.size());
        }
    }

    // Execute / close
    if (input.is_key_just_pressed(Key::Enter)) {
        execute();
    }
    if (input.is_key_just_pressed(Key::Escape)) {
        m_visible = false;
    }

    return true; // consume all input when visible
}

// ─── Command execution ────────────────────────────────────────────

void DebugConsole::execute() {
    // Trim whitespace
    auto is_space = [](char c) { return c == ' ' || c == '\t'; };
    auto start = std::find_if_not(m_input.begin(), m_input.end(), is_space);
    auto end   = std::find_if_not(m_input.rbegin(), m_input.rend(), is_space).base();
    if (start >= end) {
        // Empty line — just clear
        m_input.clear();
        m_cursor_pos = 0;
        return;
    }
    std::string cmd(start, end);

    // Save to history (avoid consecutive duplicates)
    if (m_history.empty() || m_history.back() != cmd) {
        m_history.push_back(cmd);
        if (m_history.size() > 50) {
            m_history.erase(m_history.begin());
        }
    }
    m_history_idx = -1;

    // Echo
    add_log(LogLevel::Info, "> " + cmd);

    // Tokenize by whitespace
    std::vector<std::string> args;
    {
        size_t pos = 0;
        while (pos < cmd.size()) {
            while (pos < cmd.size() && is_space(cmd[pos])) ++pos;
            if (pos >= cmd.size()) break;
            size_t tok_start = pos;
            while (pos < cmd.size() && !is_space(cmd[pos])) ++pos;
            args.push_back(cmd.substr(tok_start, pos - tok_start));
        }
    }

    if (args.empty()) {
        m_input.clear();
        m_cursor_pos = 0;
        return;
    }

    // Lookup and execute
    auto it = m_commands.find(args[0]);
    if (it == m_commands.end()) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "Unknown command: %s  (type 'help' for available commands)",
            args[0].c_str());
        add_log(LogLevel::Warn, buf);
    } else {
        it->second.fn(args);
    }

    m_input.clear();
    m_cursor_pos = 0;
}

// ─── Command registry ──────────────────────────────────────────────

void DebugConsole::register_command(const std::string& name, const std::string& help, CommandFn fn) {
    m_commands[name] = Command{help, std::move(fn)};
}

void DebugConsole::register_builtins(Engine* engine) {
    m_engine = engine;

    register_command("help", "Show available commands", [this](const std::vector<std::string>&) {
        add_log(LogLevel::Info, "Available commands:");
        for (const auto& [name, cmd] : m_commands) {
            add_log(LogLevel::Info, std::string("  ") + name + " - " + cmd.help);
        }
    });

    register_command("clear", "Clear console output", [this](const std::vector<std::string>&) {
        m_log.clear();
    });

    register_command("set", "set time_scale <0.0..10.0>", [this](const std::vector<std::string>& args) {
        if (args.size() < 3 || args[1] != "time_scale") {
            add_log(LogLevel::Warn, "Usage: set time_scale <0.0..10.0>");
            return;
        }
        if (!m_engine) {
            add_log(LogLevel::Error, "No engine reference (call register_builtins with a valid Engine*)");
            return;
        }
        f32 ts = std::stof(args[2]); // may throw, but caller catches
        ts = std::max(0.0f, std::min(ts, 10.0f));
        m_engine->set_time_scale(ts);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Time scale set to %.2f", ts);
        add_log(LogLevel::Info, buf);
    });

    register_command("toggle", "toggle <overlay|inspector|physics_aabbs|physics_pairs|physics_grid|physics_vel|prefab_viewer>", [this](const std::vector<std::string>&) {
        add_log(LogLevel::Info, "toggle: feature toggling is registered by the host application");
    });
}

// ─── Log capture ───────────────────────────────────────────────────

void DebugConsole::log_capture(LogLevel level, const char* message, void* user_data) {
    auto* self = static_cast<DebugConsole*>(user_data);
    self->add_log(level, message);
}

void DebugConsole::add_log(LogLevel level, const std::string& text) {
    m_log.push_back({level, text});
    if (m_log.size() > MAX_LOG) {
        // Simply erase oldest 10% at a time to avoid O(n) per push
        size_t remove = MAX_LOG / 10;
        if (remove == 0) remove = 1;
        m_log.erase(m_log.begin(), m_log.begin() + remove);
    }
    scroll_to_bottom();
}

// ─── Rendering ─────────────────────────────────────────────────────

void DebugConsole::render(TextRenderer& tr, Font& font, i32 window_w, i32 window_h) {
    (void)window_w;
    if (!m_visible) return;

    const f32 scale = 0.65f;
    const f32 line_h = std::ceil(font.line_height() * scale) + 2.0f;
    const f32 x = 8.0f;
    f32 y = 8.0f;

    // Title
    tr.draw_text(font, "=== DEBUG CONSOLE ===", x, y, scale, 0.3f, 0.6f, 1.0f, 1.0f);
    y += line_h;

    // Compute how many output lines fit before the input area
    i32 reserved_lines = 3; // title, separator, input
    i32 max_lines = (window_h - static_cast<i32>(y + line_h * reserved_lines)) / static_cast<i32>(line_h);
    if (max_lines < 0) max_lines = 0;

    i32 log_count = static_cast<i32>(m_log.size());
    i32 start = log_count - max_lines;
    if (start < 0) start = 0;

    for (i32 i = start; i < log_count; ++i) {
        const auto& entry = m_log[i];
        f32 r, g, b;
        switch (entry.level) {
            case LogLevel::Debug: r = 0.6f; g = 0.6f; b = 0.6f; break;
            case LogLevel::Info:  r = 1.0f; g = 1.0f; b = 1.0f; break;
            case LogLevel::Warn:  r = 1.0f; g = 0.9f; b = 0.3f; break;
            case LogLevel::Error: r = 1.0f; g = 0.3f; b = 0.3f; break;
            default:              r = 1.0f; g = 1.0f; b = 1.0f; break;
        }
        tr.draw_text(font, entry.text.c_str(), x, y, scale, r, g, b, 1.0f);
        y += line_h;
    }

    // Separator
    y += 2;
    const char* sep = "────────────────────────────────────────────────────";
    tr.draw_text(font, sep, x, y, scale, 0.4f, 0.4f, 0.4f, 1.0f);
    y += line_h - 2;

    // Input line with blinking cursor (always visible for simplicity)
    std::string display = "> " + m_input;
    if (m_cursor_pos >= 0 && m_cursor_pos <= static_cast<i32>(m_input.size())) {
        display.insert(static_cast<size_t>(2 + m_cursor_pos), 1, '_');
    }
    tr.draw_text(font, display.c_str(), x, y, scale, 0.3f, 1.0f, 0.3f, 1.0f);
}

void DebugConsole::scroll_to_bottom() {
    // Auto-scroll is handled in render() by always showing the last N lines.
    // This method is a no-op placeholder — kept for future manual scroll support.
}

} // namespace pino
