cmake_minimum_required(VERSION 3.20)

find_package(X11 REQUIRED)

add_library(SapanaEngine STATIC
    src/camera/Camera.cpp
    src/camera/CameraConfig.cpp
    src/input/Action.cpp
    src/input/InputBindings.cpp
    src/input/InputSystem.cpp
    src/input/CursorControllerLinux.cpp
)

target_include_directories(SapanaEngine
PUBLIC
    include
PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)

# BasicMath + platform macros; SampleBase for InputController; Imgui for Key.M.
target_link_libraries(SapanaEngine
PUBLIC
    Diligent-Common
    Diligent-SampleBase
PRIVATE
    Diligent-BuildSettings
    Diligent-Imgui
    X11::X11
)

set_common_target_properties(SapanaEngine)
