#pragma once

#include "engine/core/types.h"
#include "engine/ecs/prefab.h"
#include "engine/renderer/font.h"
#include "engine/renderer/text_renderer.h"
#include "engine/platform/input.h"
#include "engine/platform/file_system.h"
#include <string>
#include <vector>

namespace pino {

class AssetManager;

class PrefabDebugViewer {
public:
    PrefabDebugViewer();
    ~PrefabDebugViewer();

    PrefabDebugViewer(const PrefabDebugViewer&) = delete;
    PrefabDebugViewer& operator=(const PrefabDebugViewer&) = delete;

    void toggle();
    bool is_visible() const { return m_visible; }
    bool handle_input(Input& input);

    bool load_file(const char* path);
    void load_prefab(const Prefab& prefab);
    void unload();

    void set_asset_manager(AssetManager* assets) { m_assets = assets; }
    void set_file_system(FileSystem& fs) { m_fs = &fs; }

    void validate();
    bool has_errors() const { return !m_errors.empty(); }
    bool has_warnings() const { return !m_warnings.empty(); }

    void render(TextRenderer& tr, Font& font, i32 window_w, i32 window_h);

private:
    struct Issue {
        bool is_error;
        std::string message;
    };

    static const char* type_name(u32 hash);
    void draw_components(TextRenderer& tr, Font& font, f32 x, f32& y, f32 line_h, f32 scale);
    void draw_issues(TextRenderer& tr, Font& font, f32 x, f32& y, f32 line_h, f32 scale);

    bool m_visible = false;
    Prefab m_prefab;
    AssetManager* m_assets = nullptr;
    FileSystem*   m_fs = nullptr;
    std::vector<Issue> m_errors;
    std::vector<Issue> m_warnings;
    bool m_loaded = false;
    std::string m_source_name;
    int m_scroll = 0;
};

} // namespace pino
