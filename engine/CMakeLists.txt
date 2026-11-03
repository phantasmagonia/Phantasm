add_library(phantasm
    src/application.cpp
    src/atlas.cpp
    src/camera.cpp
    src/chunk_index.cpp
    src/frame_clock.cpp
    src/frame_stats.cpp
    src/geometry.cpp
    src/image.cpp
    src/input.cpp
    src/log.cpp
    src/paths.cpp
    src/renderer.cpp
    src/sprite.cpp
    src/sprite_batch.cpp
    src/task_pool.cpp
    src/texture.cpp
    src/window.cpp
)
add_library(phantasm::phantasm ALIAS phantasm)

find_package(Threads REQUIRED)

target_include_directories(phantasm PUBLIC include)
target_compile_features(phantasm PUBLIC cxx_std_20)
target_link_libraries(phantasm
    PRIVATE SDL3::SDL3 stb phantasm_warnings
    PUBLIC Threads::Threads
)

phantasm_add_shaders(phantasm_shaders
    shaders/sprite.vert
    shaders/sprite.frag
)
add_dependencies(phantasm phantasm_shaders)
