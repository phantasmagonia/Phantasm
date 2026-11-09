add_library(phantasm_iso
    src/iso_grid.cpp
    src/fog.cpp
    src/scene.cpp
    src/scene_history.cpp
    src/tile_map.cpp
    src/visibility.cpp
)
add_library(phantasm::iso ALIAS phantasm_iso)

target_include_directories(phantasm_iso PUBLIC include)
target_link_libraries(phantasm_iso
    PUBLIC phantasm
    PRIVATE phantasm_warnings
)
