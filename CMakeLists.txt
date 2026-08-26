cmake_minimum_required(VERSION 3.20)

# Force consistent MSVC runtime library across all targets, including third_party
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
cmake_policy(SET CMP0091 NEW)

project(SapanaEngine)

add_subdirectory(third_party/DiligentEngine)
add_subdirectory(engine)
add_subdirectory(game/sandbox_cube)