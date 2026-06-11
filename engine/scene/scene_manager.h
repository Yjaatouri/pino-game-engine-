#pragma once

#include "engine/core/types.h"
#include "engine/scene/i_scene.h"
#include <memory>
#include <vector>
#include <algorithm>

namespace pino {

class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager() { clear(); }

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // Push a new scene on top. Current scene gets on_pause(), new gets on_enter() + init().
    void push(std::unique_ptr<IScene> scene) {
        m_pending_ops.push_back({OpType::Push, std::move(scene)});
    }

    // Pop the current scene. It gets on_exit() + shutdown(), previous scene gets on_resume().
    void pop() {
        m_pending_ops.push_back({OpType::Pop, nullptr});
    }

    // Replace the current scene. Current gets on_exit() + shutdown(), new gets on_enter() + init().
    void replace(std::unique_ptr<IScene> scene) {
        m_pending_ops.push_back({OpType::Replace, std::move(scene)});
    }

    // Update the active scene (called by engine each fixed step). Must not be called during flush.
    void update(f32 dt) {
        if (auto* s = current())
            s->update(dt);
    }

    // Render the active scene (called each frame).
    void render(f32 dt) {
        if (auto* s = current())
            s->render(dt);
    }

    // Apply pending scene operations at a safe frame boundary.
    void flush() {
        for (auto& op : m_pending_ops) {
            switch (op.type) {
                case OpType::Push:
                    do_push(std::move(op.scene));
                    break;
                case OpType::Pop:
                    do_pop();
                    break;
                case OpType::Replace:
                    do_replace(std::move(op.scene));
                    break;
            }
        }
        m_pending_ops.clear();
    }

    bool empty() const { return m_stack.empty(); }
    IScene* current() const {
        return m_stack.empty() ? nullptr : m_stack.back().get();
    }

    void clear() {
        m_pending_ops.clear();
        while (!m_stack.empty())
            do_pop();
    }

private:
    enum class OpType { Push, Pop, Replace };

    struct PendingOp {
        OpType type;
        std::unique_ptr<IScene> scene;
    };

    void do_push(std::unique_ptr<IScene> scene) {
        if (!scene) return;
        if (auto* prev = current())
            prev->on_pause();
        scene->on_enter(current());
        scene->init();
        m_stack.push_back(std::move(scene));
    }

    void do_pop() {
        if (m_stack.empty()) return;
        auto scene = std::move(m_stack.back());
        m_stack.pop_back();
        scene->on_exit(current());
        scene->shutdown();
        if (auto* next = current())
            next->on_resume();
    }

    void do_replace(std::unique_ptr<IScene> scene) {
        if (!scene) return;
        if (!m_stack.empty()) {
            auto old = std::move(m_stack.back());
            m_stack.pop_back();
            old->on_exit(scene.get());
            old->shutdown();
        }
        scene->on_enter(current());
        scene->init();
        m_stack.push_back(std::move(scene));
    }

    std::vector<std::unique_ptr<IScene>> m_stack;
    std::vector<PendingOp> m_pending_ops;
};

} // namespace pino
