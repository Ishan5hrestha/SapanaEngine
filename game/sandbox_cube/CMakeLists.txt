cmake_minimum_required (VERSION 3.10)

project(Tutorial02_Cube CXX)

# Shaders stay listed so Diligent's sample helpers can track them; all runtime
# content under assets/ is synced wholesale (config, scenes, meshes, etc.).
add_sample_app(Tutorial02_Cube
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial02_Cube.cpp
    INCLUDES
        src/Tutorial02_Cube.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
)

target_link_libraries(Tutorial02_Cube PRIVATE SapanaEngine)
# nlohmann JSON used by sandbox for renderer.json
target_include_directories(Tutorial02_Cube PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

# Always mirror game/sandbox_cube/assets → next to the binary (no per-file whitelist).
add_custom_command(TARGET Tutorial02_Cube POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:Tutorial02_Cube>"
    COMMENT "Sync sandbox assets to build output"
)
