add_executable(scavenger src/main.cpp)
target_link_libraries(scavenger PRIVATE phantasm_iso phantasm_warnings)

add_custom_target(scavenger_assets ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/scavenger"
)
add_dependencies(scavenger scavenger_assets)
