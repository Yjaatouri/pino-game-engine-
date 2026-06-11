#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/texture.h"
#include "engine/platform/file_system.h"
#include <glm/glm.hpp>

namespace pino {

class SkyboxRenderer {
public:
    SkyboxRenderer() = default;
    ~SkyboxRenderer();

    SkyboxRenderer(const SkyboxRenderer&) = delete;
    SkyboxRenderer& operator=(const SkyboxRenderer&) = delete;

    SkyboxRenderer(SkyboxRenderer&& other) noexcept;
    SkyboxRenderer& operator=(SkyboxRenderer&& other) noexcept;

    // Compiles skybox shader and builds unit cube mesh.
    bool init();

    // Load a cubemap from 6 individual face textures (via FileSystem).
    // Order: +X, -X, +Y, -Y, +Z, -Z
    bool load_cubemap(FileSystem& fs,
                      const char* right, const char* left,
                      const char* top, const char* bottom,
                      const char* front, const char* back);

    // Render the skybox (must have loaded cubemap).
    // view: camera view matrix (translation will be removed internally)
    // proj: camera projection matrix
    void render(const glm::mat4& view, const glm::mat4& proj);

    void destroy();

    bool is_valid() const { return m_initialized; }

private:
    Shader m_shader;
    Mesh   m_cube;
    Texture m_cubemap;
    bool   m_initialized = false;
};

} // namespace pino
