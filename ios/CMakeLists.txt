cmake_minimum_required(VERSION 3.21)
project(pino_engine_ios LANGUAGES CXX C OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Engine source root
set(ENGINE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../engine)

# ─── Dependencies via FetchContent ───────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        31c1ad3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc10
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tinyobjloader)

# ─── Frameworks ──────────────────────────────────────────────────────────────
set(IOS_FRAMEWORKS
    "-framework UIKit"
    "-framework OpenGLES"
    "-framework QuartzCore"
    "-framework Foundation"
    "-framework Metal"
)

# ─── Engine library ──────────────────────────────────────────────────────────
add_library(pino STATIC
    # Core
    ${ENGINE_ROOT}/core/log.cpp
    ${ENGINE_ROOT}/core/logger.cpp
    ${ENGINE_ROOT}/core/config_loader.cpp
    ${ENGINE_ROOT}/core/transform.cpp
    ${ENGINE_ROOT}/core/math_utils.cpp

    # Platform — iOS backend (no SDL2)
    ${ENGINE_ROOT}/platform/ios/ios_window.mm
    ${ENGINE_ROOT}/platform/ios/ios_input.mm
    ${ENGINE_ROOT}/platform/ios/ios_file_system.mm
    ${ENGINE_ROOT}/platform/ios/ios_platform.mm

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
    ${ENGINE_ROOT}/renderer/metal_renderer.mm

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

    # Physics
    ${ENGINE_ROOT}/physics/collision_world.cpp
    ${ENGINE_ROOT}/physics/debug_draw.cpp
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
    PRIVATE
        ${IOS_FRAMEWORKS}
)

# ─── Game app bundle ─────────────────────────────────────────────────────────
set(GAME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/ios_lit_cube/main.m
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/ios_lit_cube/AppDelegate.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/ios_lit_cube/EAGLView.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/ios_lit_cube/ViewController.mm
)

add_executable(pino_game MACOSX_BUNDLE ${GAME_SOURCES})

target_include_directories(pino_game
    PRIVATE
        ${ENGINE_ROOT}/..
)

target_link_libraries(pino_game
    pino
    ${IOS_FRAMEWORKS}
)

set_target_properties(pino_game PROPERTIES
    MACOSX_BUNDLE                        TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER         "com.pino.engine"
    MACOSX_BUNDLE_BUNDLE_VERSION         "1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING   "1.0"
    MACOSX_BUNDLE_INFO_PLIST             ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY   "Apple Development"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "14.0"
    XCODE_ATTRIBUTE_ENABLE_BITCODE       "NO"
)

# Copy assets to the app bundle Resources directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/assets)

# Add a post-build step to copy assets into the .app bundle
add_custom_command(TARGET pino_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:pino_game>/assets
    COMMENT "Copying assets to app bundle"
)

# Copy launch screen and asset catalogue into bundle
add_custom_command(TARGET pino_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/LaunchScreen.storyboard
        $<TARGET_FILE_DIR:pino_game>/LaunchScreen.storyboard
    COMMENT "Copying launch screen"
)

# Alias for consistency with desktop build
add_library(engine_core ALIAS pino)
