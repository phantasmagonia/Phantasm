add_library(phantasm
    src/application.cpp
    src/frame_clock.cpp
    src/input.cpp
    src/log.cpp
    src/renderer.cpp
    src/window.cpp
)
add_library(phantasm::phantasm ALIAS phantasm)

target_include_directories(phantasm PUBLIC include)
target_compile_features(phantasm PUBLIC cxx_std_20)
target_link_libraries(phantasm
    PRIVATE SDL3::SDL3 phantasm_warnings
)
