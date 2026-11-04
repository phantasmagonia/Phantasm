# Editor library: desktop tooling only. Runtime and mobile builds never link it.
add_library(phantasm_editor_lib
    src/asset_cache.cpp
    src/editor.cpp
    src/editor_state.cpp
)
add_library(phantasm::editor ALIAS phantasm_editor_lib)
target_include_directories(phantasm_editor_lib PUBLIC include)
# ImGui is public: games draw their own panels and inspectors with it.
target_link_libraries(phantasm_editor_lib
    PUBLIC phantasm_iso phantasm_imgui
    PRIVATE phantasm_warnings
)

add_executable(phantasm_editor app/main.cpp)
target_link_libraries(phantasm_editor PRIVATE phantasm_editor_lib phantasm_warnings)
