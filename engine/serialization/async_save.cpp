#include "engine/serialization/async_save.h"

namespace pino {

AsyncSaveSerializer::AsyncSaveSerializer()
    : m_progress(0.0f)
    , m_entities_written(0)
    , m_done(false)
    , m_serializer(m_types, m_versions, m_strings)
    , m_ser(m_writer)
{
}

AsyncSaveSerializer::~AsyncSaveSerializer() {
    if (m_thread.joinable()) {
        m_thread.detach();
    }
}

void AsyncSaveSerializer::start_async(EcsScene& scene) {
    SaveGameSerializer::registerTypes(m_types);
    SaveGameSerializer::registerVersions(m_versions);

    m_progress.store(0.0f, std::memory_order_release);
    m_entities_written.store(0, std::memory_order_release);
    m_done.store(false, std::memory_order_release);
    m_writer.clear();

    m_thread = std::thread(&AsyncSaveSerializer::thread_func, this, &scene);
}

void AsyncSaveSerializer::thread_func(EcsScene* scene) {
    // Capture entity count early so progress denominator is stable.
    uint32_t total = scene->entity_count();

    m_serializer.serialize(m_ser, *scene,
        [&](uint32_t current, uint32_t) {
            m_entities_written.store(current, std::memory_order_release);
            if (total > 0) {
                float pct = static_cast<float>(current) / static_cast<float>(total);
                m_progress.store(pct, std::memory_order_release);
            }
        });

    m_progress.store(1.0f, std::memory_order_release);
    m_done.store(true, std::memory_order_release);
}

const std::vector<uint8_t>& AsyncSaveSerializer::finish() {
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_buffer = m_writer.getBuffer();
    return m_buffer;
}

} // namespace pino
