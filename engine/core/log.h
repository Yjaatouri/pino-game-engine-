#pragma once

#include "logger.h"
#include <cstdlib>

namespace pino {

// ─── Backward-compat free functions ───────────────────────────
// These delegate to the Logger singleton.

inline void log_set_level(LogLevel level) { Logger::set_level(level); }
inline LogLevel log_get_level() { return Logger::level(); }

} // namespace pino

// ─── Legacy PINO_LOG macros (keep working) ────────────────────
#define PINO_LOG(level, fmt, ...) \
    ::pino::Logger::write(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// Debug stripped in release builds
#if !defined(NDEBUG) || (defined(_DEBUG))
#define PINO_DEBUG(fmt, ...)  PINO_LOG(::pino::LogLevel::Debug, fmt, ##__VA_ARGS__)
#else
#define PINO_DEBUG(fmt, ...)  ((void)0)
#endif

#define PINO_INFO(fmt, ...)   PINO_LOG(::pino::LogLevel::Info,  fmt, ##__VA_ARGS__)
#define PINO_WARN(fmt, ...)   PINO_LOG(::pino::LogLevel::Warn,  fmt, ##__VA_ARGS__)
#define PINO_ERROR(fmt, ...)  PINO_LOG(::pino::LogLevel::Error, fmt, ##__VA_ARGS__)

// ─── Assertions ───────────────────────────────────────────────
#if !defined(NDEBUG) || (defined(_DEBUG))
#define ENGINE_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            ::pino::Logger::write(::pino::LogLevel::Error, __FILE__, __LINE__, \
                "ENGINE_ASSERT FAILED: %s", #cond); \
            std::abort(); \
        } \
    } while (0)

#define ENGINE_ASSERT_MSG(cond, msg) \
    do { \
        if (!(cond)) { \
            ::pino::Logger::write(::pino::LogLevel::Error, __FILE__, __LINE__, \
                "ENGINE_ASSERT FAILED: %s — %s", #cond, msg); \
            std::abort(); \
        } \
    } while (0)
#else
#define ENGINE_ASSERT(cond)     ((void)0)
#define ENGINE_ASSERT_MSG(cond, msg) ((void)0)
#endif
