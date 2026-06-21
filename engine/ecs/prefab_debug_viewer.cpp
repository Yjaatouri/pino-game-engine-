#include "engine/ecs/prefab_debug_viewer.h"
#include "engine/ecs/components.h"
#include "engine/assets/asset_manager.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <cstdio>
#include <cmath>

namespace pino {

PrefabDebugViewer::PrefabDebugViewer() = default;
PrefabDebugViewer::~PrefabDebugViewer() = default;

void PrefabDebugViewer::toggle() { m_visible = !m_visible; }

bool PrefabDebugViewer::handle_input(Input& input) {
    if (input.is_key_just_pressed(Key::F9)) {
        toggle();
        return true;
    }
    return false;
}

bool PrefabDebugViewer::load_file(const char* path) {
    bool ok = m_prefab.load(path);
    if (ok) {
        m_loaded = true;
        m_source_name = path;
        m_errors.clear();
        m_warnings.clear();
        validate();
    }
    return ok;
}

void PrefabDebugViewer::load_prefab(const Prefab& prefab) {
    m_prefab = prefab;
    m_loaded = true;
    m_source_name = "(memory)";
    m_errors.clear();
    m_warnings.clear();
    validate();
}

void PrefabDebugViewer::unload() {
    m_loaded = false;
    m_source_name.clear();
    m_errors.clear();
    m_warnings.clear();
    m_prefab = Prefab();
}

const char* PrefabDebugViewer::type_name(u32 hash) {
    if (hash == kRenderComponentHash)  return "RenderComponent";
    if (hash == kPhysicsComponentHash) return "PhysicsComponent";
    if (hash == kAudioComponentHash)   return "AudioComponent";
    return "Unknown";
}

void PrefabDebugViewer::validate() {
    m_errors.clear();
    m_warnings.clear();

    if (!m_loaded) return;

    // Validate transform
    if (m_prefab.has_transform()) {
        const auto& t = m_prefab.transform();
        if (std::isnan(t.position.x) || std::isnan(t.position.y) || std::isnan(t.position.z))
            m_errors.push_back({true, "Transform position contains NaN"});
        if (std::isnan(t.rotation.x) || std::isnan(t.rotation.y) || std::isnan(t.rotation.z) || std::isnan(t.rotation.w))
            m_errors.push_back({true, "Transform rotation contains NaN"});
        if (t.scale.x <= 0.0f || t.scale.y <= 0.0f || t.scale.z <= 0.0f)
            m_warnings.push_back({false, "Transform scale has non-positive components"});
    }

    // Validate each component
    for (const auto& entry : m_prefab.components()) {
        if (entry.type_hash == kRenderComponentHash) {
            if (entry.data.size() != sizeof(RenderComponent)) {
                m_errors.push_back({true, "RenderComponent data size mismatch (expected " +
                    std::to_string(sizeof(RenderComponent)) + ", got " +
                    std::to_string(entry.data.size()) + ")"});
            }
        } else if (entry.type_hash == kPhysicsComponentHash) {
            if (entry.data.size() != sizeof(PhysicsComponent)) {
                m_errors.push_back({true, "PhysicsComponent data size mismatch (expected " +
                    std::to_string(sizeof(PhysicsComponent)) + ", got " +
                    std::to_string(entry.data.size()) + ")"});
            } else {
                PhysicsComponent pc;
                memcpy(&pc, entry.data.data(), sizeof(PhysicsComponent));
                if (pc.local_min.x > pc.local_max.x || pc.local_min.y > pc.local_max.y || pc.local_min.z > pc.local_max.z)
                    m_errors.push_back({true, "PhysicsComponent: local_min exceeds local_max"});
                if (pc.collision_layer == 0)
                    m_warnings.push_back({false, "PhysicsComponent: collision_layer is 0 (no collisions)"});
            }
        } else if (entry.type_hash == kAudioComponentHash) {
            if (entry.data.size() != sizeof(AudioComponent)) {
                m_errors.push_back({true, "AudioComponent data size mismatch (expected " +
                    std::to_string(sizeof(AudioComponent)) + ", got " +
                    std::to_string(entry.data.size()) + ")"});
            }
        }
    }

    // Check asset paths
    bool has_render = false;
    bool has_audio = false;
    for (const auto& entry : m_prefab.components()) {
        if (entry.type_hash == kRenderComponentHash) has_render = true;
        if (entry.type_hash == kAudioComponentHash) has_audio = true;
    }

    if (has_render && m_prefab.mesh_path().empty())
        m_warnings.push_back({false, "RenderComponent present but no mesh path set"});

    if (has_audio && m_prefab.sound_path().empty())
        m_warnings.push_back({false, "AudioComponent present but no sound path set"});

    // Check asset existence via AssetManager
    if (m_assets && has_render && !m_prefab.mesh_path().empty()) {
        // Check if the mesh asset exists in the asset manager's cache
        // We use a simple existence check via raw file lookup
        if (!m_assets->filesystem().exists(m_prefab.mesh_path().c_str()))
            m_warnings.push_back({false, "Mesh path not found: " + m_prefab.mesh_path()});
    }

    if (m_assets && has_audio && !m_prefab.sound_path().empty()) {
        if (!m_assets->filesystem().exists(m_prefab.sound_path().c_str()))
            m_warnings.push_back({false, "Sound path not found: " + m_prefab.sound_path()});
    }
}

void PrefabDebugViewer::render(TextRenderer& tr, Font& font, i32 window_w, i32 window_h) {
    (void)window_w;
    (void)window_h;
    if (!m_visible) return;

    const f32 scale = 0.65f;
    const f32 x = 12.0f;
    const f32 line_h = std::ceil(font.line_height() * scale) + 2.0f;
    f32 y = 10.0f;

    tr.draw_text(font, "=== PREFAB DEBUG ===", x, y, scale, 0.3f, 0.9f, 0.9f, 1.0f);
    y += line_h;

    if (!m_loaded) {
        tr.draw_text(font, "No prefab loaded  [F9: toggle]", x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
        return;
    }

    char buf[256];

    // Source
    std::snprintf(buf, sizeof(buf), "Source: %s  [F9: toggle]", m_source_name.c_str());
    tr.draw_text(font, buf, x, y, scale, 0.6f, 0.6f, 0.6f, 1.0f);
    y += line_h;
    y += 2;

    // Transform
    tr.draw_text(font, "Transform", x, y, scale, 0.5f, 0.8f, 1.0f, 1.0f);
    y += line_h;
    if (m_prefab.has_transform()) {
        const auto& t = m_prefab.transform();
        glm::vec3 euler = glm::degrees(glm::eulerAngles(t.rotation));
        std::snprintf(buf, sizeof(buf), "  Pos: %.2f %.2f %.2f", t.position.x, t.position.y, t.position.z);
        tr.draw_text(font, buf, x, y, scale, 0.9f, 0.9f, 0.9f, 1.0f);
        y += line_h;
        std::snprintf(buf, sizeof(buf), "  Rot: %.1f %.1f %.1f (deg)", euler.x, euler.y, euler.z);
        tr.draw_text(font, buf, x, y, scale, 0.9f, 0.9f, 0.9f, 1.0f);
        y += line_h;
        std::snprintf(buf, sizeof(buf), "  Scl: %.2f %.2f %.2f", t.scale.x, t.scale.y, t.scale.z);
        tr.draw_text(font, buf, x, y, scale, 0.9f, 0.9f, 0.9f, 1.0f);
        y += line_h;
    } else {
        tr.draw_text(font, "  (none)", x, y, scale, 0.6f, 0.6f, 0.6f, 1.0f);
        y += line_h;
    }
    y += 2;

    // Components
    draw_components(tr, font, x, y, line_h, scale);

    // Asset paths
    if (!m_prefab.mesh_path().empty() || !m_prefab.sound_path().empty()) {
        y += 2;
        tr.draw_text(font, "Asset References", x, y, scale, 0.5f, 0.8f, 1.0f, 1.0f);
        y += line_h;
        if (!m_prefab.mesh_path().empty()) {
            std::snprintf(buf, sizeof(buf), "  Mesh: %s", m_prefab.mesh_path().c_str());
            tr.draw_text(font, buf, x, y, scale, 0.9f, 0.9f, 0.9f, 1.0f);
            y += line_h;
        }
        if (!m_prefab.sound_path().empty()) {
            std::snprintf(buf, sizeof(buf), "  Sound: %s", m_prefab.sound_path().c_str());
            tr.draw_text(font, buf, x, y, scale, 0.9f, 0.9f, 0.9f, 1.0f);
            y += line_h;
        }
    }

    // Issues
    draw_issues(tr, font, x, y, line_h, scale);
}

void PrefabDebugViewer::draw_components(TextRenderer& tr, Font& font, f32 x, f32& y, f32 line_h, f32 scale) {
    const auto& comps = m_prefab.components();
    tr.draw_text(font, "Components", x, y, scale, 0.5f, 0.8f, 1.0f, 1.0f);
    y += line_h;

    if (comps.empty()) {
        tr.draw_text(font, "  (none)", x, y, scale, 0.6f, 0.6f, 0.6f, 1.0f);
        y += line_h;
        return;
    }

    char buf[256];

    for (size_t i = 0; i < comps.size(); ++i) {
        const auto& entry = comps[i];
        const char* name = type_name(entry.type_hash);

        std::snprintf(buf, sizeof(buf), "  [%zu] %s (%u bytes)", i, name, (u32)entry.data.size());
        tr.draw_text(font, buf, x, y, scale, 1.0f, 0.9f, 0.3f, 1.0f);
        y += line_h;

        if (entry.type_hash == kRenderComponentHash && entry.data.size() == sizeof(RenderComponent)) {
            RenderComponent rc;
            memcpy(&rc, entry.data.data(), sizeof(RenderComponent));
            std::snprintf(buf, sizeof(buf), "    mesh: %s", m_prefab.mesh_path().empty() ? "(unset)" : m_prefab.mesh_path().c_str());
            tr.draw_text(font, buf, x, y, scale, 0.8f, 0.8f, 0.8f, 1.0f);
            y += line_h;
            std::snprintf(buf, sizeof(buf), "    material: %p  transparent: %s",
                          (const void*)rc.material, rc.transparent ? "yes" : "no");
            tr.draw_text(font, buf, x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
            y += line_h;
        } else if (entry.type_hash == kPhysicsComponentHash && entry.data.size() == sizeof(PhysicsComponent)) {
            PhysicsComponent pc;
            memcpy(&pc, entry.data.data(), sizeof(PhysicsComponent));
            std::snprintf(buf, sizeof(buf), "    AABB: (%.2f,%.2f,%.2f) - (%.2f,%.2f,%.2f)",
                          pc.local_min.x, pc.local_min.y, pc.local_min.z,
                          pc.local_max.x, pc.local_max.y, pc.local_max.z);
            tr.draw_text(font, buf, x, y, scale, 0.8f, 0.8f, 0.8f, 1.0f);
            y += line_h;
            std::snprintf(buf, sizeof(buf), "    static: %s  enabled: %s  layer: %u  mask: %u",
                          pc.is_static ? "yes" : "no",
                          pc.enabled ? "yes" : "no",
                          pc.collision_layer, pc.collision_mask);
            tr.draw_text(font, buf, x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
            y += line_h;
            std::snprintf(buf, sizeof(buf), "    velocity: (%.2f, %.2f, %.2f)",
                          pc.velocity.x, pc.velocity.y, pc.velocity.z);
            tr.draw_text(font, buf, x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
            y += line_h;
        } else if (entry.type_hash == kAudioComponentHash && entry.data.size() == sizeof(AudioComponent)) {
            AudioComponent ac;
            memcpy(&ac, entry.data.data(), sizeof(AudioComponent));
            std::snprintf(buf, sizeof(buf), "    path: %s", m_prefab.sound_path().empty() ? "(unset)" : m_prefab.sound_path().c_str());
            tr.draw_text(font, buf, x, y, scale, 0.8f, 0.8f, 0.8f, 1.0f);
            y += line_h;
            std::snprintf(buf, sizeof(buf), "    volume: %.2f  looping: %s  spatial: %s",
                          ac.volume, ac.looping ? "yes" : "no", ac.spatial ? "yes" : "no");
            tr.draw_text(font, buf, x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
            y += line_h;
        } else {
            // Unknown component — hex dump first few bytes
            u32 show = (std::min)(entry.data.size(), (usize)16);
            std::snprintf(buf, sizeof(buf), "    data (first %u bytes):", show);
            tr.draw_text(font, buf, x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
            y += line_h;
            for (u32 j = 0; j < show; j += 8) {
                char hex[32];
                int pos = 0;
                for (u32 k = j; k < (std::min)(j + 8, show); ++k)
                    pos += std::snprintf(hex + pos, sizeof(hex) - pos, "%02x ", entry.data[k]);
                std::snprintf(buf, sizeof(buf), "      %s", hex);
                tr.draw_text(font, buf, x, y, scale, 0.6f, 0.6f, 0.6f, 1.0f);
                y += line_h;
            }
        }
        y += 1;
    }
}

void PrefabDebugViewer::draw_issues(TextRenderer& tr, Font& font, f32 x, f32& y, f32 line_h, f32 scale) {
    y += 2;
    if (m_errors.empty() && m_warnings.empty()) {
        tr.draw_text(font, "Validation: PASS (no issues)", x, y, scale, 0.3f, 0.9f, 0.3f, 1.0f);
        y += line_h;
        return;
    }

    char buf[256];
    tr.draw_text(font, "Validation Issues:", x, y, scale, 1.0f, 1.0f, 0.3f, 1.0f);
    y += line_h;

    for (const auto& issue : m_errors) {
        std::snprintf(buf, sizeof(buf), "  [ERROR] %s", issue.message.c_str());
        tr.draw_text(font, buf, x, y, scale, 1.0f, 0.3f, 0.3f, 1.0f);
        y += line_h;
    }
    for (const auto& issue : m_warnings) {
        std::snprintf(buf, sizeof(buf), "  [WARN]  %s", issue.message.c_str());
        tr.draw_text(font, buf, x, y, scale, 1.0f, 0.8f, 0.2f, 1.0f);
        y += line_h;
    }
}

} // namespace pino
