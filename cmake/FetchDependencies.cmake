include(FetchContent)

# ---------------------------------------------------------------------------
# SDL2 — windowing, input, and basic file I/O
# ---------------------------------------------------------------------------
if(PINO_USE_SYSTEM_SDL2)
    find_package(SDL2 REQUIRED QUIET)
    if(NOT SDL2_FOUND)
        message(FATAL_ERROR "PINO_USE_SYSTEM_SDL2 is set but SDL2 was not found.")
    endif()
else()
    FetchContent_Declare(
        sdl2
        GIT_REPOSITORY  https://github.com/libsdl-org/SDL.git
        GIT_TAG         release-2.26.5
        GIT_SHALLOW     TRUE
    )
    # SDL 2.26 ships a CMakeLists.txt — just pull it in.
    # Disable shared lib build — we link statically.
    set(SDL_SHARED_ENABLED_BY_DEFAULT OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC_ENABLED_BY_DEFAULT ON  CACHE BOOL "" FORCE)
    set(SDL_TEST_ENABLED_BY_DEFAULT  OFF CACHE BOOL "" FORCE)
    set(SDL_SOUND_ENABLED_BY_DEFAULT OFF CACHE BOOL "" FORCE)

    # Avoid pulling in unnecessary back-ends on desktop
    if(WIN32)
        set(SDL_HIDAPI          ON  CACHE BOOL "" FORCE)
        set(SDL_DIRECTINPUT     OFF CACHE BOOL "" FORCE)
    elseif(APPLE)
        # macOS / iOS — keep Metal disabled for now (GL path)
    endif()

    FetchContent_MakeAvailable(sdl2)

    # SDL2::SDL2 target is created by SDL's own CMake
    if(NOT TARGET SDL2::SDL2)
        message(FATAL_ERROR "SDL2 CMake target was not created properly.")
    endif()
endif()

# ---------------------------------------------------------------------------
# GLM — header-only maths library
# ---------------------------------------------------------------------------
FetchContent_Declare(
    glm
    GIT_REPOSITORY  https://github.com/g-truc/glm.git
    GIT_TAG         0.9.9.8
    GIT_SHALLOW     TRUE
)
FetchContent_MakeAvailable(glm)

# ---------------------------------------------------------------------------
# tinyobjloader — single-header .obj mesh loader
# ---------------------------------------------------------------------------
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY  https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG         v2.0.0rc10
    GIT_SHALLOW     TRUE
)
FetchContent_MakeAvailable(tinyobjloader)

# ---------------------------------------------------------------------------
# miniaudio — single-header audio library (header + .c split coming in 0.12)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY  https://github.com/mackron/miniaudio.git
    GIT_TAG         0.11.25
    GIT_SHALLOW     TRUE
)

# miniaudio builds a static library; we'll link against it and
# provide the implementation macro in our own translation unit.
FetchContent_MakeAvailable(miniaudio)

# ---------------------------------------------------------------------------
# stb — single-header image loader
# ---------------------------------------------------------------------------
FetchContent_Declare(
    stb
    GIT_REPOSITORY  https://github.com/nothings/stb.git
    # Pinned commit for reproducible builds (2024-05-31)
    GIT_TAG         31c1ad3
    GIT_SHALLOW     TRUE
)
FetchContent_MakeAvailable(stb)
