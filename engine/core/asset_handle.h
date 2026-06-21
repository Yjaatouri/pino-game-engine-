#pragma once

#include "engine/core/types.h"
#include <memory>

namespace pino {

template <typename T>
class AssetHandle {
public:
    AssetHandle() = default;
    explicit AssetHandle(std::shared_ptr<T> ptr) : m_ptr(std::move(ptr)) {}

    bool is_loaded() const { return m_ptr != nullptr; }
    T*   get()       const { return m_ptr.get(); }
    T*   operator->() const { return m_ptr.get(); }
    T&   operator*()  const { return *m_ptr; }
    explicit operator bool() const { return is_loaded(); }

    const T& value_or(const T& fallback) const {
        return m_ptr ? *m_ptr : fallback;
    }

private:
    std::shared_ptr<T> m_ptr;
};

} // namespace pino
