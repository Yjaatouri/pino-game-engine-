#pragma once

#include "engine/core/types.h"
#include "engine/ecs/ecs_scene.h"
#include "engine/renderer/font.h"
#include "engine/renderer/text_renderer.h"
#include "engine/platform/input.h"
#include <vector>

namespace pino {

class ECSInspector {
public:
    ECSInspector();
    ~ECSInspector();

    ECSInspector(const ECSInspector&) = delete;
    ECSInspector& operator=(const ECSInspector&) = delete;

    void toggle();
    bool is_visible() const { return m_visible; }
    bool handle_input(Input& input);

    void set_scene(EcsScene* scene) { m_scene = scene; }

    void render(TextRenderer& tr, Font& font, i32 window_w, i32 window_h);

private:
    enum class Page { EntityList, ComponentView };

    void refresh_list();
    void draw_entity_list(TextRenderer& tr, Font& font, f32 x, f32& y, f32 line_h, f32 scale);
    void draw_component_view(TextRenderer& tr, Font& font, f32 x, f32& y, f32 line_h, f32 scale);

    bool m_visible = false;
    EcsScene* m_scene = nullptr;
    Page m_page = Page::EntityList;

    std::vector<EntityId> m_entities;
    int m_selected = -1;
    int m_list_scroll = 0;
    int m_focus = 0;
};

} // namespace pino
