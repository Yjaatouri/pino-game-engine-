# Helper: simple example without assets
function(add_pino_example name)
    add_executable(${name} ${name}/main.cpp)
    target_link_libraries(${name} PRIVATE engine_core)
endfunction()

# Helper: example with assets (engine resolves asset root at runtime)
function(add_pino_example_assets name)
    add_executable(${name} ${name}/main.cpp)
    target_link_libraries(${name} PRIVATE engine_core)
endfunction()

# ─── Simple examples (no assets) ───────────────────────────────
add_pino_example(triangle)
add_pino_example(clear)
add_pino_example(cube)
add_pino_example(fps_camera)
add_pino_example(scene_graph)

# ─── Asset-using examples ─────────────────────────────────────
add_pino_example_assets(asset_demo)
add_pino_example_assets(lit_scene)
add_pino_example_assets(game_demo)
add_pino_example_assets(input_test)
add_pino_example_assets(collision_demo)
add_pino_example_assets(arena_game)
add_pino_example_assets(demo_game)
add_pino_example_assets(stability_test)
