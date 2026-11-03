cmake_minimum_required(VERSION 3.26)

project(Phantasm VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Executables, shaders, and assets share one folder so runtime paths are simple.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(PHANTASM_BUILD_SANDBOX "Build the sandbox demo app" ON)
option(PHANTASM_BUILD_TESTS "Build unit tests" ON)

add_library(phantasm_warnings INTERFACE)
if(MSVC)
    target_compile_options(phantasm_warnings INTERFACE /W4 /WX /permissive-)
else()
    target_compile_options(phantasm_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

include(cmake/Dependencies.cmake)
include(cmake/Shaders.cmake)

add_subdirectory(engine)
add_subdirectory(iso)

if(PHANTASM_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()

if(PHANTASM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
