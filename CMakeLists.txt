cmake_minimum_required(VERSION 3.26)

project(Phantasm VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Executables, shaders, and assets share one folder so runtime paths are simple.
# A parent project that includes Phantasm may choose its own folder.
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

# Extras default to ON only when Phantasm is the top-level project, so games that
# pull in Phantasm don't build its tests, benchmarks, or sandbox.
option(PHANTASM_BUILD_PHYSICS "Build the phantasm_physics library (Box2D)" ON)
option(PHANTASM_BUILD_SANDBOX "Build the sandbox demo app" ${PROJECT_IS_TOP_LEVEL})
option(PHANTASM_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(PHANTASM_BUILD_BENCHMARKS "Build performance benchmarks" ${PROJECT_IS_TOP_LEVEL})

add_library(phantasm_warnings INTERFACE)
if(MSVC)
    target_compile_options(phantasm_warnings INTERFACE /W4 /WX /permissive-)
else()
    target_compile_options(phantasm_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # GCC rejects out-of-order designated initializers; Clang only warns when asked.
    target_compile_options(phantasm_warnings INTERFACE -Wreorder-init-list)
endif()

include(cmake/Dependencies.cmake)
include(cmake/Shaders.cmake)

add_subdirectory(engine)
add_subdirectory(iso)
if(PHANTASM_BUILD_PHYSICS)
    add_subdirectory(physics)
endif()

if(PHANTASM_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()

if(PHANTASM_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(PHANTASM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
