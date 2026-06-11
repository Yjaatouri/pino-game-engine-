cmake_minimum_required(VERSION 3.21)
project(pino VERSION 0.1.0 LANGUAGES CXX)

# Allow FetchContent dependencies that target older CMake versions
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options
include(cmake/options.cmake)

# Export compile commands for IDE / tooling integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Organized output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler warnings
include(cmake/compiler_warnings.cmake)

# Sanitizers (debug builds only)
include(cmake/sanitizers.cmake)

# Dependencies
include(cmake/FetchDependencies.cmake)

# Engine library
add_subdirectory(engine)

# Copy assets to the build directory for easy run-target access
file(COPY ${CMAKE_SOURCE_DIR}/examples/assets/ DESTINATION ${CMAKE_BINARY_DIR}/assets)

# Examples
if(PINO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ─── Run target ────────────────────────────────────────────────
add_custom_target(run
    COMMAND $<TARGET_FILE:game_demo>
    DEPENDS game_demo
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    USES_TERMINAL
)

# ─── Install ───────────────────────────────────────────────────
install(TARGETS engine_core platform_desktop
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/examples/assets/ DESTINATION assets)
