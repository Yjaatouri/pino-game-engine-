add_executable(asset_cooker
    main.cpp
    cooker.cpp
    mesh_cooker.cpp
    texture_cooker.cpp
    shader_cooker.cpp
    stb_image_impl.cpp
    ${CMAKE_SOURCE_DIR}/engine/core/log.cpp
    ${CMAKE_SOURCE_DIR}/engine/core/logger.cpp
    ${CMAKE_SOURCE_DIR}/engine/core/binary_chunk.cpp
    ${CMAKE_SOURCE_DIR}/engine/core/serializer.cpp
    ${CMAKE_SOURCE_DIR}/engine/serialization/cooked_asset.cpp
)

target_include_directories(asset_cooker
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${stb_SOURCE_DIR}
        ${tinyobjloader_SOURCE_DIR}
)

target_link_libraries(asset_cooker
    PRIVATE
        glm::glm
        tinyobjloader
)
