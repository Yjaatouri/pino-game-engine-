#pragma once

#include "types.h"
#include <cstdio>
#include <mutex>

namespace pino {

enum class LogLevel : u32 {
    Debug,
    Info,
    Warn,
    Error,
};

class Logger {
public:
    static void init(const char* log_path = "engine.log");
    static void shutdown();

    static void set_level(LogLevel level);
    static LogLevel level();

    static void write(LogLevel level, const char* file, int line, const char* fmt, ...);

    // ── Log capture callback (for debug console, etc.) ─────────
    using LogCallback = void(*)(LogLevel level, const char* message, void* user_data);
    static void set_callback(LogCallback cb, void* user_data = nullptr);

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void write_impl(LogLevel level, const char* file, int line, const char* fmt, va_list args);

    static Logger& inst();

    std::mutex m_mutex;
    LogLevel   m_level = LogLevel::Debug;
    FILE*      m_file  = nullptr;
    bool       m_owned = false;
    LogCallback m_callback = nullptr;
    void*      m_callback_data = nullptr;
};

} // namespace pino
