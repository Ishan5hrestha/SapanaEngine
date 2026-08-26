cmake_minimum_required(VERSION 3.20)

find_package(X11 REQUIRED)

include(FetchContent)

# Jolt Physics (pinned). Do not let Jolt override our Diligent/project compiler flags.
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "Jolt: do not override CMAKE_CXX_FLAGS" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "Jolt: disable LTO for Diligent compatibility" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "Jolt: keep warnings off for dependency" FORCE)
set(USE_AVX512 OFF CACHE BOOL "" FORCE)
set(CPP_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
set(CPP_RTTI_ENABLED OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG        v5.3.0
    SOURCE_SUBDIR  Build
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JoltPhysics)

add_library(SapanaEngine STATIC
    src/camera/Camera.cpp
    src/camera/CameraConfig.cpp
    src/input/Action.cpp
    src/input/InputBindings.cpp
    src/input/InputSystem.cpp
    src/input/CursorControllerLinux.cpp
    src/assets/PrimitiveMeshes.cpp
    src/assets/AssetCache.cpp
    src/assets/GltfMeshLoader.cpp
    src/scene/SceneLoader.cpp
    src/render/BasicForwardRenderer.cpp
    src/render/PbrGltfRenderer.cpp
    src/physics/PhysicsConfig.cpp
    src/physics/PhysicsSystem.cpp
)

target_include_directories(SapanaEngine
PUBLIC
    include
PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)

# EnTT is fetched by DiligentFX; Jolt is PRIVATE (opaque behind Sapana physics API).
target_link_libraries(SapanaEngine
PUBLIC
    Diligent-Common
    Diligent-SampleBase
    EnTT::EnTT
PRIVATE
    Diligent-BuildSettings
    Diligent-Imgui
    Diligent-AssetLoader
    Diligent-TextureLoader
    DiligentFX
    Jolt
    X11::X11
)

set_common_target_properties(SapanaEngine)
