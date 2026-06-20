#pragma once

#include "engine/core/types.h"
#include "engine/core/log.h"

namespace pino {

#if !defined(NDEBUG) || defined(PINO_FORCE_VALIDATION)
    #define PINO_CHECK(cond, msg) \
        do { \
            if (!(cond)) { \
                PINO_ERROR("CHECK FAILED: %s - %s", #cond, msg); \
                ENGINE_ASSERT_MSG(cond, msg); \
            } \
        } while(0)

    #define PINO_ENSURE(cond) \
        do { \
            if (!(cond)) { \
                PINO_ERROR("ENSURE FAILED: %s", #cond); \
                ENGINE_ASSERT(cond); \
            } \
        } while(0)
#else
    #define PINO_CHECK(cond, msg) ((void)0)
    #define PINO_ENSURE(cond) ((void)0)
#endif

#define PINO_REQUIRE(cond, msg) \
    do { \
        if (!(cond)) { \
            PINO_ERROR("REQUIRE FAILED: %s - %s", #cond, msg); \
            ENGINE_ASSERT_MSG(cond, msg); \
        } \
    } while(0)

}
