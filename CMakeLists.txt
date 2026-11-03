cmake_minimum_required(VERSION 3.24)

project(Phantasm VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PHANTASM_BUILD_SANDBOX "Build the sandbox demo app" ON)
option(PHANTASM_BUILD_TESTS "Build unit tests" ON)

add_library(phantasm_warnings INTERFACE)
if(MSVC)
    target_compile_options(phantasm_warnings INTERFACE /W4 /WX /permissive-)
else()
    target_compile_options(phantasm_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

include(cmake/Dependencies.cmake)

add_subdirectory(engine)

if(PHANTASM_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()

if(PHANTASM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
