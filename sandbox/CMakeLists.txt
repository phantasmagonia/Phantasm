add_executable(sandbox main.cpp)
target_link_libraries(sandbox PRIVATE phantasm phantasm_warnings)

add_custom_target(sandbox_assets ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets"
)
add_dependencies(sandbox sandbox_assets)
