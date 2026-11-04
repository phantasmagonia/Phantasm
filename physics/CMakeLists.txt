add_library(phantasm_physics
    src/world.cpp
)
add_library(phantasm::physics ALIAS phantasm_physics)

target_include_directories(phantasm_physics PUBLIC include)
# Box2D is PRIVATE: public headers never include it.
target_link_libraries(phantasm_physics
    PUBLIC phantasm
    PRIVATE box2d phantasm_warnings
)
