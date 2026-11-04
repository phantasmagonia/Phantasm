# Head-to-head benchmark: the same sprite workload on Phantasm, raylib, SFML, and
# SDL3's SDL_Renderer. Standalone project; never part of Phantasm's normal build.
#
#   cmake -S benchmarks/compare -B build/compare -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/compare && benchmarks/compare/run.sh build/compare
cmake_minimum_required(VERSION 3.26)
project(PhantasmCompare LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Phantasm itself (from this checkout) also provides SDL3 for the SDL_Renderer program.
set(PHANTASM_BUILD_PHYSICS OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../.." phantasm)

include(FetchContent)
# raylib (zlib)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git
                     GIT_TAG 6.2 GIT_SHALLOW TRUE)
# SFML (zlib); graphics only
set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SFML GIT_REPOSITORY https://github.com/SFML/SFML.git
                     GIT_TAG 3.1.0 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib SFML)

add_executable(compare_phantasm phantasm_main.cpp)
target_link_libraries(compare_phantasm PRIVATE phantasm)

add_executable(compare_raylib raylib_main.cpp)
target_link_libraries(compare_raylib PRIVATE raylib)

add_executable(compare_sfml sfml_main.cpp)
target_link_libraries(compare_sfml PRIVATE SFML::Graphics)

add_executable(compare_sdl_renderer sdl_renderer_main.cpp)
target_link_libraries(compare_sdl_renderer PRIVATE SDL3::SDL3)

# OpenGL is deprecated on macOS but is what raylib and SFML use there.
target_compile_definitions(compare_raylib PRIVATE GL_SILENCE_DEPRECATION)
target_compile_definitions(compare_sfml PRIVATE GL_SILENCE_DEPRECATION)
