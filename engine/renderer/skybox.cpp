#include "skybox.h"
#include "engine/core/log.h"
#include "engine/renderer/render_state.h"
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cstring>

namespace pino {

SkyboxRenderer::~SkyboxRenderer() { destroy(); }

SkyboxRenderer::SkyboxRenderer(SkyboxRenderer&& other) noexcept
    : m_shader(std::move(other.m_shader)),
      m_cube(std::move(other.m_cube)),
      m_cubemap(std::move(other.m_cubemap)),
      m_initialized(other.m_initialized)
{
    other.m_initialized = false;
}

SkyboxRenderer& SkyboxRenderer::operator=(SkyboxRenderer&& other) noexcept {
    if (this != &other) {
        destroy();
        m_shader = std::move(other.m_shader);
        m_cube = std::move(other.m_cube);
        m_cubemap = std::move(other.m_cubemap);
        m_initialized = other.m_initialized;
        other.m_initialized = false;
    }
    return *this;
}

bool SkyboxRenderer::init() {
    destroy();

    static const char* sky_vert = R"(
#version 300 es
layout(location=0) in vec3 a_pos;
uniform mat4 u_vp_no_trans;
out vec3 v_uvw;
void main() {
    v_uvw = a_pos;
    vec4 p = u_vp_no_trans * vec4(a_pos, 1.0);
    gl_Position = p.xyww;
})";

    static const char* sky_frag = R"(
#version 300 es
precision highp float;
uniform samplerCube u_cubemap;
in vec3 v_uvw;
out vec4 frag_color;
void main() {
    frag_color = texture(u_cubemap, v_uvw);
})";

    if (!m_shader.load(sky_vert, sky_frag)) {
        PINO_ERROR("Skybox shader failed to compile");
        return false;
    }

    // Unit cube mesh
    struct V { glm::vec3 pos; glm::vec3 nrm; glm::vec2 uv; };
    std::vector<V> verts;
    std::vector<u32> idx;
    auto emit = [&](const glm::vec3& p, const glm::vec3& n, const glm::vec2& uv) {
        verts.push_back({p, n, uv});
        idx.push_back(static_cast<u32>(idx.size()));
    };

    emit({ 1,-1,-1},{ 1,0,0},{1,1}); emit({ 1, 1,-1},{ 1,0,0},{1,0});
    emit({ 1, 1, 1},{ 1,0,0},{0,0}); emit({ 1,-1, 1},{ 1,0,0},{0,1});
    emit({-1,-1, 1},{-1,0,0},{1,1}); emit({-1, 1, 1},{-1,0,0},{1,0});
    emit({-1, 1,-1},{-1,0,0},{0,0}); emit({-1,-1,-1},{-1,0,0},{0,1});
    emit({-1, 1,-1},{0, 1,0},{1,1}); emit({ 1, 1,-1},{0, 1,0},{1,0});
    emit({ 1, 1, 1},{0, 1,0},{0,0}); emit({-1, 1, 1},{0, 1,0},{0,1});
    emit({-1,-1, 1},{0,-1,0},{1,1}); emit({ 1,-1, 1},{0,-1,0},{1,0});
    emit({ 1,-1,-1},{0,-1,0},{0,0}); emit({-1,-1,-1},{0,-1,0},{0,1});
    emit({-1,-1, 1},{0,0, 1},{1,1}); emit({ 1,-1, 1},{0,0, 1},{1,0});
    emit({ 1, 1, 1},{0,0, 1},{0,0}); emit({-1, 1, 1},{0,0, 1},{0,1});
    emit({ 1,-1,-1},{0,0,-1},{1,1}); emit({-1,-1,-1},{0,0,-1},{1,0});
    emit({-1, 1,-1},{0,0,-1},{0,0}); emit({ 1, 1,-1},{0,0,-1},{0,1});

    std::vector<u32> indices;
    for (u32 i = 0; i < 24; i += 4) {
        indices.push_back(i);   indices.push_back(i+1); indices.push_back(i+2);
        indices.push_back(i);   indices.push_back(i+2); indices.push_back(i+3);
    }

    m_cube.upload(reinterpret_cast<const Vertex*>(verts.data()),
                  static_cast<u32>(verts.size()),
                  indices.data(), static_cast<u32>(indices.size()));

    m_initialized = true;
    return true;
}

bool SkyboxRenderer::load_cubemap(FileSystem& fs,
                                   const char* right, const char* left,
                                   const char* top, const char* bottom,
                                   const char* front, const char* back) {
    if (!m_initialized) {
        PINO_ERROR("SkyboxRenderer not initialized");
        return false;
    }

    const char* face_paths[6] = {right, left, top, bottom, front, back};

    // Load each face and decode to RGBA
    struct { std::vector<u8> rgba; i32 w, h; } faces[6];

    for (i32 i = 0; i < 6; ++i) {
        std::vector<u8> file_data = fs.read_binary(face_paths[i]);
        if (file_data.empty()) {
            PINO_ERROR("Failed to read skybox face: %s", face_paths[i]);
            return false;
        }

        i32 channels = 0;
        stbi_set_flip_vertically_on_load(0);
        unsigned char* pixels = stbi_load_from_memory(
            file_data.data(), static_cast<i32>(file_data.size()),
            &faces[i].w, &faces[i].h, &channels, 4);
        if (!pixels) {
            PINO_ERROR("stb_image failed to decode: %s", face_paths[i]);
            return false;
        }

        usize pixel_count = static_cast<usize>(faces[i].w * faces[i].h * 4);
        faces[i].rgba.assign(pixels, pixels + pixel_count);
        stbi_image_free(pixels);
    }

    // Upload cubemap
    const u8* face_ptr[6];
    for (i32 i = 0; i < 6; ++i)
        face_ptr[i] = faces[i].rgba.data();

    // All faces should be same size
    i32 fw = faces[0].w, fh = faces[0].h;
    if (!m_cubemap.create_cubemap(face_ptr, fw, fh)) {
        PINO_ERROR("Failed to create cubemap texture");
        return false;
    }

    PINO_INFO("Skybox cubemap loaded (%dx%d)", fw, fh);
    return true;
}

void SkyboxRenderer::render(const glm::mat4& view, const glm::mat4& proj) {
    if (!m_initialized || !m_cubemap.is_valid()) return;

    auto& rs = RenderState::instance();

    rs.push_state();
    rs.set_depth_test(true);
    rs.set_depth_func(GL_LEQUAL);
    rs.set_depth_write(false);

    // Remove translation from view matrix
    glm::mat4 view_rot = glm::mat4(glm::mat3(view));
    glm::mat4 vp_no_trans = proj * view_rot;

    m_shader.bind();
    m_shader.set_mat4("u_vp_no_trans", vp_no_trans);

    m_cubemap.bind(0);
    m_shader.set_int("u_cubemap", 0);

    m_cube.draw();
    rs.pop_state();
}

void SkyboxRenderer::destroy() {
    m_shader.destroy();
    m_cube.destroy();
    m_cubemap.destroy();
    m_initialized = false;
}

} // namespace pino
