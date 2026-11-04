add_executable(phantasm_editor main.cpp)
target_link_libraries(phantasm_editor PRIVATE phantasm_iso phantasm_imgui SDL3::SDL3 phantasm_warnings)
