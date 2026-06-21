# ─── Desktop platform library ───────────────────────────────────
add_library(platform_desktop STATIC
    platform/sdl2/sdl2_window.cpp
    platform/sdl2/sdl2_input.cpp
    platform/sdl2/sdl2_file_system.cpp
    platform/sdl2/sdl2_platform.cpp
)

target_include_directories(platform_desktop
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(platform_desktop
    PUBLIC SDL2::SDL2
)

if(WIN32)
    target_link_libraries(platform_desktop PRIVATE opengl32)
elseif(APPLE)
    find_library(PINO_OPENGL_FW OpenGL)
    if(PINO_OPENGL_FW)
        target_link_libraries(platform_desktop PRIVATE "${PINO_OPENGL_FW}")
    endif()
elseif(UNIX AND NOT APPLE)
    target_link_libraries(platform_desktop PRIVATE GL)
endif()

# ─── Engine core library ────────────────────────────────────────
set(PINO_ENGINE_SOURCES
    core/log.cpp
    core/logger.cpp
    core/config_loader.cpp
    core/transform.cpp
    core/math_utils.cpp
    core/engine_context.cpp
    renderer/gl_es3.cpp
    renderer/shader.cpp
    renderer/mesh.cpp
    renderer/texture.cpp
    renderer/camera.cpp
    renderer/fps_controller.cpp
    renderer/font.cpp
    renderer/light.cpp
    renderer/render_stats.cpp
    renderer/framebuffer.cpp
    renderer/render_state.cpp
    renderer/per_frame_ubo.cpp
    renderer/debug_renderer.cpp
    renderer/text_renderer.cpp
    renderer/shadow_map.cpp
    renderer/skybox.cpp
    renderer/material.cpp
    renderer/render_queue.cpp
    renderer/frustum.cpp
    renderer/lod_group.cpp
    assets/stb_image.cpp
    assets/asset_manager.cpp
    assets/asset_pack.cpp
    scene/entity.cpp
    scene/scene.cpp
    input/gamepad.cpp
    audio/audio_manager.cpp
    ecs/prefab.cpp
    ecs/ecs_inspector.cpp
    renderer/debug_overlay.cpp
    engine.cpp
)

if(PINO_ENABLE_PHYSICS)
    list(APPEND PINO_ENGINE_SOURCES
        physics/collision_world.cpp
        physics/debug_draw.cpp
    )
endif()

add_library(engine_core STATIC ${PINO_ENGINE_SOURCES})

target_include_directories(engine_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE ${stb_SOURCE_DIR} ${tinyobjloader_SOURCE_DIR}
)

target_link_libraries(engine_core
    PUBLIC
        platform_desktop
        glm::glm
        miniaudio
    PRIVATE
        miniaudio_reverb_node
)

# Suppress MSVC CRT deprecation warnings (localtime, etc.)
if(MSVC)
    target_compile_definitions(engine_core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Engine version (passed through CMake project(... VERSION ...))
target_compile_definitions(engine_core PRIVATE PINO_VERSION="${PROJECT_VERSION}")

# ─── Backward compatible alias ─────────────────────────────────
add_library(pino ALIAS engine_core)
