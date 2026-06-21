#pragma once

#include "engine/core/types.h"

namespace pino {

// Lightweight stable entity identifier.
// An entity's identity never changes after creation.
// After destruction, the slot is recycled with a bumped generation,
// so stale references are detectable.
struct EntityId {
    u32 index      = UINT32_MAX;
    u32 generation = 0;

    bool operator==(EntityId o) const { return index == o.index && generation == o.generation; }
    bool operator!=(EntityId o) const { return !(*this == o); }
    explicit operator bool() const { return index != UINT32_MAX; }
};

static constexpr EntityId NullEntity = {UINT32_MAX, 0};

} // namespace pino
