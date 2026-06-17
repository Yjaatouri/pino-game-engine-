cmake_minimum_required(VERSION 3.21)
project(pino_engine_android LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ─── Engine source root ──────────────────────────────────────────────────────
set(ENGINE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../engine)

# ─── Dependencies via FetchContent ───────────────────────────────────────────
include(FetchContent)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# stb
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        31c1ad3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

# tinyobjloader
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc10
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tinyobjloader)

# ─── Engine library ──────────────────────────────────────────────────────────
add_library(pino STATIC
    # Core
    ${ENGINE_ROOT}/core/log.cpp
    ${ENGINE_ROOT}/core/logger.cpp
    ${ENGINE_ROOT}/core/transform.cpp
    ${ENGINE_ROOT}/core/math_utils.cpp
    ${ENGINE_ROOT}/core/config_loader.cpp

    # Platform — Android backend (no SDL2)
    ${ENGINE_ROOT}/platform/android/android_window.cpp
    ${ENGINE_ROOT}/platform/android/android_input.cpp
    ${ENGINE_ROOT}/platform/android/android_file_system.cpp
    ${ENGINE_ROOT}/platform/android/android_platform.cpp

    # Renderer
    ${ENGINE_ROOT}/renderer/gl_es3.cpp
    ${ENGINE_ROOT}/renderer/shader.cpp
    ${ENGINE_ROOT}/renderer/mesh.cpp
    ${ENGINE_ROOT}/renderer/texture.cpp
    ${ENGINE_ROOT}/renderer/camera.cpp
    ${ENGINE_ROOT}/renderer/fps_controller.cpp
    ${ENGINE_ROOT}/renderer/light.cpp
    ${ENGINE_ROOT}/renderer/render_stats.cpp
    ${ENGINE_ROOT}/renderer/framebuffer.cpp
    ${ENGINE_ROOT}/renderer/render_state.cpp
    ${ENGINE_ROOT}/renderer/per_frame_ubo.cpp
    ${ENGINE_ROOT}/renderer/debug_renderer.cpp
    ${ENGINE_ROOT}/renderer/shadow_map.cpp
    ${ENGINE_ROOT}/renderer/skybox.cpp

    # Assets
    ${ENGINE_ROOT}/assets/stb_image.cpp
    ${ENGINE_ROOT}/assets/asset_manager.cpp
    ${ENGINE_ROOT}/assets/asset_pack.cpp

    # Scene
    ${ENGINE_ROOT}/scene/entity.cpp
    ${ENGINE_ROOT}/scene/scene.cpp

    # Input
    ${ENGINE_ROOT}/input/gamepad.cpp

    # Engine
    ${ENGINE_ROOT}/engine.cpp
)

target_include_directories(pino
    PUBLIC
        ${ENGINE_ROOT}/..
    PRIVATE
        ${stb_SOURCE_DIR}
        ${tinyobjloader_SOURCE_DIR}
)

target_link_libraries(pino
    PUBLIC
        glm::glm
        EGL
        GLESv3
        android
        log
)

# ─── Game executable ─────────────────────────────────────────────────────────
add_library(pino_game SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/android_lit_cube/main.cpp
)

target_include_directories(pino_game
    PRIVATE
        ${ENGINE_ROOT}/..
)

target_link_libraries(pino_game
    pino
    android
    log
    native_app_glue
)

set_target_properties(pino_game PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Build-type specific defines
if(PINO_BUILD_TYPE STREQUAL "debug")
    target_compile_definitions(pino PRIVATE _DEBUG)
    target_compile_options(pino PRIVATE -g -O0)
else()
    target_compile_definitions(pino PRIVATE NDEBUG)
    target_compile_options(pino PRIVATE -O2)
endif()

# Tell Gradle about the shared library
set_target_properties(pino_game PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
)

# Alias for consistency with desktop build
add_library(engine_core ALIAS pino)
