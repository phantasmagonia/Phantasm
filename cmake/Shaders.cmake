# Compiles GLSL to SPIR-V (Vulkan) and MSL (Metal) next to the executables.
find_program(GLSLC_EXECUTABLE glslc REQUIRED)
find_program(SPIRV_CROSS_EXECUTABLE spirv-cross REQUIRED)

function(phantasm_add_shaders target)
    set(output_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
    set(outputs "")
    foreach(source IN LISTS ARGN)
        get_filename_component(name "${source}" NAME)
        set(spv "${output_dir}/${name}.spv")
        set(msl "${output_dir}/${name}.msl")
        add_custom_command(
            OUTPUT "${spv}" "${msl}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
            COMMAND "${GLSLC_EXECUTABLE}" -O "${CMAKE_CURRENT_SOURCE_DIR}/${source}" -o "${spv}"
            COMMAND "${SPIRV_CROSS_EXECUTABLE}" --msl "${spv}" --output "${msl}"
            DEPENDS "${source}"
            COMMENT "Compiling shader ${name}"
            VERBATIM
        )
        list(APPEND outputs "${spv}" "${msl}")
    endforeach()
    add_custom_target(${target} ALL DEPENDS ${outputs})
endfunction()
