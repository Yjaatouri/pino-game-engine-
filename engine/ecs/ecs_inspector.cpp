#include "engine/ecs/ecs_inspector.h"
#include "engine/ecs/components.h"
#include <glm/gtc/quaternion.hpp>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace pino {

ECSInspector::ECSInspector() = default;
ECSInspector::~ECSInspector() = default;

void ECSInspector::toggle() { m_visible = !m_visible; }

bool ECSInspector::handle_input(Input& input) {
    if (input.is_key_just_pressed(Key::F4)) {
        toggle();
        if (m_visible) refresh_list();
        return true;
    }
    if (!m_visible) return false;

    switch (m_page) {
    case Page::EntityList:
        if (input.is_key_just_pressed(Key::Down)) {
            if (m_selected < static_cast<int>(m_entities.size()) - 1) {
                ++m_selected;
                if (m_selected - m_list_scroll >= 20) ++m_list_scroll;
            }
        }
        if (input.is_key_just_pressed(Key::Up)) {
            if (m_selected > 0) {
                --m_selected;
                if (m_selected < m_list_scroll) --m_list_scroll;
            }
        }
        if (input.is_key_just_pressed(Key::Right) || input.is_key_just_pressed(Key::Enter)) {
            if (m_selected >= 0 && m_selected < static_cast<int>(m_entities.size())) {
                m_page = Page::ComponentView;
                m_focus = 0;
            }
        }
        break;

    case Page::ComponentView:
        if (input.is_key_just_pressed(Key::Left) || input.is_key_just_pressed(Key::Escape)) {
            m_page = Page::EntityList;
            refresh_list();
        }
        if (input.is_key_just_pressed(Key::Down)) {
            m_focus = std::min(m_focus + 1, 1);
        }
        if (input.is_key_just_pressed(Key::Up)) {
            m_focus = std::max(m_focus - 1, 0);
        }
        if (input.is_key_just_pressed(Key::Enter)) {
            if (!m_scene || m_selected < 0) break;
            EntityId e = m_entities[m_selected];
            if (!m_scene->alive(e)) break;

            if (m_focus == 0) {
                RenderComponent* rc = m_scene->get_component<RenderComponent>(e);
                if (rc) rc->enabled = !rc->enabled;
            } else if (m_focus == 1) {
                PhysicsComponent* pc = m_scene->get_component<PhysicsComponent>(e);
                if (pc) pc->enabled = !pc->enabled;
            }
        }
        break;
    }

    return false;
}

void ECSInspector::refresh_list() {
    m_entities.clear();
    if (!m_scene) return;
    m_scene->registry().each([&](EntityId id) {
        m_entities.push_back(id);
    });
    if (m_selected >= static_cast<int>(m_entities.size()))
        m_selected = static_cast<int>(m_entities.size()) - 1;
    if (m_selected < 0 && !m_entities.empty())
        m_selected = 0;
}

void ECSInspector::render(TextRenderer& tr, Font& font, i32 window_w, i32 window_h) {
    (void)window_h;
    if (!m_visible) return;

    refresh_list();

    const f32 scale = 0.65f;
    const f32 x = 12.0f;
    const f32 line_h = std::ceil(font.line_height() * scale) + 2.0f;
    f32 y = 10.0f;

    tr.draw_text(font, "=== ECS INSPECTOR ===", x, y, scale, 0.3f, 0.6f, 1.0f, 1.0f);
    y += line_h;

    if (!m_scene) {
        tr.draw_text(font, "No scene attached", x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
        return;
    }

    char buf[128];
    std::snprintf(buf, sizeof(buf), "Entities: %zu  [F4: toggle, arrows: nav, Enter: select, Esc: back]",
                  m_entities.size());
    tr.draw_text(font, buf, x, y, scale, 0.6f, 0.6f, 0.6f, 1.0f);
    y += line_h;
    y += 2;

    switch (m_page) {
    case Page::EntityList:
        draw_entity_list(tr, font, x, y, line_h, scale);
        break;
    case Page::ComponentView:
        draw_component_view(tr, font, x, y, line_h, scale);
        break;
    }
}

void ECSInspector::draw_entity_list(TextRenderer& tr, Font& font, f32 x, f32& y, f32 line_h, f32 scale) {
    char buf[64];
    int count = 0;
    int max_visible = 40;

    for (int i = m_list_scroll; i < static_cast<int>(m_entities.size()) && count < max_visible; ++i, ++count) {
        auto e = m_entities[i];
        bool selected = (i == m_selected);

        std::snprintf(buf, sizeof(buf), "%s Entity #%u (gen:%u)",
                      selected ? ">" : " ", e.index, e.generation);

        if (selected) {
            tr.draw_text(font, buf, x, y, scale, 1.0f, 0.9f, 0.3f, 1.0f);
        } else {
            tr.draw_text(font, buf, x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
        }
        y += line_h;
    }
}

void ECSInspector::draw_component_view(TextRenderer& tr, Font& font, f32 x, f32& y, f32 line_h, f32 scale) {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_entities.size())) {
        tr.draw_text(font, "No entity selected", x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
        return;
    }

    EntityId e = m_entities[m_selected];
    if (!m_scene->alive(e)) {
        tr.draw_text(font, "Entity is no longer alive", x, y, scale, 1.0f, 0.3f, 0.3f, 1.0f);
        return;
    }

    char buf[192];
    std::snprintf(buf, sizeof(buf), "Entity #%u (gen:%u)", e.index, e.generation);
    tr.draw_text(font, buf, x, y, scale, 1.0f, 0.9f, 0.3f, 1.0f);
    y += line_h;
    y += 2;

    // ── Transform ──
    tr.draw_text(font, "Transform", x, y, scale, 0.5f, 0.8f, 1.0f, 1.0f);
    y += line_h;

    Transform* t = m_scene->scene_graph().get(e);
    if (t) {
        glm::vec3 euler = glm::degrees(glm::eulerAngles(t->rotation));
        std::snprintf(buf, sizeof(buf), "  Pos  %.2f  %.2f  %.2f",
                      t->position.x, t->position.y, t->position.z);
        tr.draw_text(font, buf, x, y, scale, 0.9f, 0.9f, 0.9f, 1.0f);
        y += line_h;
        std::snprintf(buf, sizeof(buf), "  Rot  %.1f  %.1f  %.1f",
                      euler.x, euler.y, euler.z);
        tr.draw_text(font, buf, x, y, scale, 0.9f, 0.9f, 0.9f, 1.0f);
        y += line_h;
        std::snprintf(buf, sizeof(buf), "  Scl  %.2f  %.2f  %.2f",
                      t->scale.x, t->scale.y, t->scale.z);
        tr.draw_text(font, buf, x, y, scale, 0.9f, 0.9f, 0.9f, 1.0f);
        y += line_h;
    } else {
        tr.draw_text(font, "  (no transform)", x, y, scale, 0.6f, 0.6f, 0.6f, 1.0f);
        y += line_h;
    }
    y += 2;

    // ── RenderComponent ──
    RenderComponent* rc = m_scene->get_component<RenderComponent>(e);
    bool focus_r = (m_focus == 0);

    if (rc) {
        std::snprintf(buf, sizeof(buf), "RenderComponent %s [%s]",
                      focus_r ? ">" : " ",
                      rc->enabled ? "ENABLED" : "DISABLED");
        tr.draw_text(font, buf, x, y, scale,
                     focus_r ? 1.0f : 0.7f,
                     rc->enabled ? (focus_r ? 0.9f : 0.7f) : 0.4f,
                     rc->enabled ? (focus_r ? 0.3f : 0.4f) : 0.4f,
                     1.0f);
        y += line_h;

        std::snprintf(buf, sizeof(buf), "  Mesh: %s", rc->mesh.is_loaded() ? "loaded" : "null");
        tr.draw_text(font, buf, x, y, scale, 0.8f, 0.8f, 0.8f, 1.0f);
        y += line_h;

        std::snprintf(buf, sizeof(buf), "  Transparent: %s", rc->transparent ? "yes" : "no");
        tr.draw_text(font, buf, x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
        y += line_h;
    } else {
        std::snprintf(buf, sizeof(buf), "RenderComponent %s [none]", focus_r ? ">" : " ");
        tr.draw_text(font, buf, x, y, scale, focus_r ? 1.0f : 0.5f, 0.5f, 0.5f, 1.0f);
        y += line_h;
    }
    y += 2;

    // ── PhysicsComponent ──
    PhysicsComponent* pc = m_scene->get_component<PhysicsComponent>(e);
    bool focus_p = (m_focus == 1);

    if (pc) {
        std::snprintf(buf, sizeof(buf), "PhysicsComponent %s [%s]",
                      focus_p ? ">" : " ",
                      pc->enabled ? "ENABLED" : "DISABLED");
        tr.draw_text(font, buf, x, y, scale,
                     focus_p ? 1.0f : 0.7f,
                     pc->enabled ? (focus_p ? 0.9f : 0.7f) : 0.4f,
                     pc->enabled ? (focus_p ? 0.3f : 0.4f) : 0.4f,
                     1.0f);
        y += line_h;

        std::snprintf(buf, sizeof(buf), "  AABB: (%.2f,%.2f,%.2f) - (%.2f,%.2f,%.2f)",
                      pc->local_min.x, pc->local_min.y, pc->local_min.z,
                      pc->local_max.x, pc->local_max.y, pc->local_max.z);
        tr.draw_text(font, buf, x, y, scale, 0.8f, 0.8f, 0.8f, 1.0f);
        y += line_h;

        std::snprintf(buf, sizeof(buf), "  Static: %s  Layer: %u  Mask: %u",
                      pc->is_static ? "yes" : "no", pc->collision_layer, pc->collision_mask);
        tr.draw_text(font, buf, x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
        y += line_h;
    } else {
        std::snprintf(buf, sizeof(buf), "PhysicsComponent %s [none]", focus_p ? ">" : " ");
        tr.draw_text(font, buf, x, y, scale, focus_p ? 1.0f : 0.5f, 0.5f, 0.5f, 1.0f);
        y += line_h;
    }
    y += 2;

    // ── AudioComponent ──
    AudioComponent* ac = m_scene->get_component<AudioComponent>(e);
    if (ac) {
        std::snprintf(buf, sizeof(buf), "AudioComponent: %s", ac->sound_path.c_str());
        tr.draw_text(font, buf, x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
        y += line_h;
        std::snprintf(buf, sizeof(buf), "  Volume: %.2f  Loop: %s  Spatial: %s",
                      ac->volume, ac->looping ? "yes" : "no", ac->spatial ? "yes" : "no");
        tr.draw_text(font, buf, x, y, scale, 0.7f, 0.7f, 0.7f, 1.0f);
        y += line_h;
    } else {
        tr.draw_text(font, "AudioComponent: [none]", x, y, scale, 0.5f, 0.5f, 0.5f, 1.0f);
        y += line_h;
    }

    y += line_h;
    tr.draw_text(font, "Enter: toggle  Left/Esc: back", x, y, scale, 0.5f, 0.5f, 0.5f, 1.0f);
}

} // namespace pino
