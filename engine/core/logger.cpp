#include "logger.h"

#include <cstdarg>
#include <ctime>

#if defined(__ANDROID__)
#include <android/log.h>
#endif

namespace pino {

// ─── Platform helpers ──────────────────────────────────────────

#if defined(__ANDROID__)
static android_LogPriority to_android(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return ANDROID_LOG_DEBUG;
        case LogLevel::Info:  return ANDROID_LOG_INFO;
        case LogLevel::Warn:  return ANDROID_LOG_WARN;
        case LogLevel::Error: return ANDROID_LOG_ERROR;
        default:              return ANDROID_LOG_INFO;
    }
}
#endif

static const char* level_label(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        default:              return "?????";
    }
}

// ─── Singleton ─────────────────────────────────────────────────

Logger& Logger::inst() {
    static Logger s_logger;
    return s_logger;
}

Logger::Logger() = default;
Logger::~Logger() { shutdown(); }

// ─── Public static API ─────────────────────────────────────────

void Logger::init(const char* log_path) {
    auto& self = inst();
    std::lock_guard<std::mutex> lock(self.m_mutex);

    if (self.m_owned) return;

#if !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
    FILE* f = std::fopen(log_path, "w");
    if (f) {
        self.m_file = f;
        self.m_owned = true;
    }
#else
    (void)log_path;
#endif
}

void Logger::shutdown() {
    auto& self = inst();
    std::lock_guard<std::mutex> lock(self.m_mutex);

    if (self.m_owned && self.m_file) {
        std::fprintf(self.m_file, "[SHUTDOWN] Logger closed\n");
        std::fclose(self.m_file);
        self.m_file  = nullptr;
        self.m_owned = false;
    }
}

void Logger::set_level(LogLevel level) {
    inst().m_level = level;
}

LogLevel Logger::level() {
    return inst().m_level;
}

void Logger::write(LogLevel level, const char* file, int line, const char* fmt, ...) {
    auto& self = inst();
    if (level < self.m_level) return;

    va_list args;
    va_start(args, fmt);
    self.write_impl(level, file, line, fmt, args);
    va_end(args);
}

// ─── Internal ──────────────────────────────────────────────────

void Logger::write_impl(LogLevel level, const char* file, int line, const char* fmt, va_list args) {
#if defined(__ANDROID__)
    va_list args_copy;
    va_copy(args_copy, args);
    __android_log_vprint(to_android(level), "PinoEngine", fmt, args_copy);
    va_end(args_copy);
#else
    std::time_t now = std::time(nullptr);
    char ts[32] = {};
    std::strftime(ts, sizeof(ts), "%H:%M:%S", std::localtime(&now));

    // Console output
    std::fprintf(stderr, "[%s] %s %s:%d | ", ts, level_label(level), file, line);
    std::vfprintf(stderr, fmt, args);
    std::fprintf(stderr, "\n");

    // File output
    if (m_file) {
        std::fprintf(m_file, "[%s] %s %s:%d | ", ts, level_label(level), file, line);

        va_list args_file;
        va_copy(args_file, args);
        std::vfprintf(m_file, fmt, args_file);
        va_end(args_file);

        std::fprintf(m_file, "\n");
    }

    // Auto-flush on ERROR
    if (level == LogLevel::Error) {
        std::fflush(stderr);
        if (m_file) std::fflush(m_file);
    }
#endif
}

} // namespace pino
