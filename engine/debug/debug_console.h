#pragma once

#include "engine/core/types.h"
#include "engine/core/logger.h"
#include "engine/engine.h"
#include "engine/renderer/font.h"
#include "engine/renderer/text_renderer.h"
#include "engine/platform/input.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace pino {

class DebugConsole {
public:
    DebugConsole();
    ~DebugConsole();

    void toggle();
    bool is_visible() const { return m_visible; }
    bool handle_input(Input& input);
    void render(TextRenderer& tr, Font& font, i32 window_w, i32 window_h);

    // Command registry
    using CommandFn = std::function<void(const std::vector<std::string>& args)>;
    void register_command(const std::string& name, const std::string& help, CommandFn fn);

    // Wire up built-in commands that need engine access
    void register_builtins(Engine* engine);

    // Log capture callback (registered with Logger::set_callback)
    static void log_capture(LogLevel level, const char* message, void* user_data);

private:
    struct LogEntry {
        LogLevel level;
        std::string text;
    };

    void execute();
    void add_log(LogLevel level, const std::string& text);
    void scroll_to_bottom();

    bool m_visible = false;

    // Output buffer (ring-like, capped at MAX_LOG)
    static constexpr usize MAX_LOG = 200;
    std::vector<LogEntry> m_log;

    // Input
    std::string m_input;
    i32 m_cursor_pos = 0;

    // History
    std::vector<std::string> m_history;
    i32 m_history_idx = -1;

    // Commands
    struct Command {
        std::string help;
        CommandFn fn;
    };
    std::unordered_map<std::string, Command> m_commands;

    // External references for built-in commands
    Engine* m_engine = nullptr;
};

} // namespace pino
