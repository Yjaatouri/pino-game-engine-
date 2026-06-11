#pragma once

#include "engine/core/types.h"
#include <functional>
#include <vector>
#include <algorithm>

namespace pino {

struct TimerHandle {
    u32 id = 0;
    bool is_valid() const { return id != 0; }
    explicit operator bool() const { return is_valid(); }
};

class TimerManager {
public:
    TimerManager() = default;
    ~TimerManager() = default;

    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;

    // Call callback once after `delay` seconds.
    TimerHandle after(f32 delay, std::function<void()> callback) {
        return add_timer(delay, std::move(callback), 1);
    }

    // Call callback every `interval` seconds. times = -1 means infinite.
    TimerHandle every(f32 interval, std::function<void()> callback, i32 times = -1) {
        return add_timer(interval, std::move(callback), times);
    }

    // Cancel a pending timer by handle.
    void cancel(TimerHandle handle) {
        if (!handle.is_valid()) return;
        m_pending_removals.push_back(handle.id);
    }

    // Cancel all timers that match a predicate.
    template <typename F>
    void cancel_if(F&& pred) {
        for (auto& t : m_timers) {
            if (pred(t.id, t.callback))
                m_pending_removals.push_back(t.id);
        }
    }

    // Advance all timers by dt seconds. If active is false, timers do not advance.
    void update(f32 dt, bool active = true) {
        process_pending();

        if (!active) return;

        for (auto& t : m_timers) {
            t.elapsed += dt;
            while (t.elapsed >= t.interval && t.remaining != 0) {
                t.elapsed -= t.interval;
                if (t.callback) t.callback();
                if (t.remaining > 0) {
                    t.remaining -= 1;
                    if (t.remaining == 0)
                        m_pending_removals.push_back(t.id);
                }
            }
        }

        process_pending();
    }

    // Remove all timers.
    void clear() {
        m_timers.clear();
        m_pending_removals.clear();
        m_pending_additions.clear();
    }

    u32 count() const { return static_cast<u32>(m_timers.size()); }

private:
    struct Timer {
        u32 id;
        f32 interval;
        f32 elapsed = 0.0f;
        i32 remaining; // -1 = infinite
        std::function<void()> callback;
    };

    TimerHandle add_timer(f32 interval, std::function<void()> callback, i32 times) {
        Timer t;
        t.id = m_next_id++;
        t.interval = interval;
        t.remaining = times;
        t.callback = std::move(callback);
        TimerHandle h{t.id};
        m_pending_additions.push_back(std::move(t));
        return h;
    }

    void process_pending() {
        // Process removals first
        if (!m_pending_removals.empty()) {
            for (u32 id : m_pending_removals) {
                auto it = std::find_if(m_timers.begin(), m_timers.end(),
                    [id](const Timer& t) { return t.id == id; });
                if (it != m_timers.end())
                    m_timers.erase(it);
            }
            m_pending_removals.clear();
        }

        // Then additions
        if (!m_pending_additions.empty()) {
            for (auto& t : m_pending_additions)
                m_timers.push_back(std::move(t));
            m_pending_additions.clear();
        }
    }

    u32 m_next_id = 1;
    std::vector<Timer> m_timers;
    std::vector<u32> m_pending_removals;
    std::vector<Timer> m_pending_additions;
};

} // namespace pino
