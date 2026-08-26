cmake_minimum_required (VERSION 3.10)

project(SapanaSandbox CXX)

# Shaders stay listed so Diligent's sample helpers can track them; all runtime
# content under assets/ is synced wholesale (config, scenes, meshes, etc.).
add_sample_app(SapanaSandbox
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial02_Cube.cpp
    INCLUDES
        src/Tutorial02_Cube.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
        assets/shaders/sky.vsh
        assets/shaders/sky.psh
        assets/shaders/shadow_depth.vsh
        assets/shaders/shadow_depth.psh
)

target_link_libraries(SapanaSandbox PRIVATE SapanaEngine)
# nlohmann JSON used by sandbox for renderer.json
target_include_directories(SapanaSandbox PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

# Always mirror game/sandbox_cube/assets → next to the binary (no per-file whitelist).
add_custom_command(TARGET SapanaSandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:SapanaSandbox>"
    COMMENT "Sync sandbox assets to build output"
)
