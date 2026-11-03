add_executable(phantasm_benchmarks
    iso_bench.cpp
    sprite_batch_bench.cpp
)
target_link_libraries(phantasm_benchmarks
    PRIVATE phantasm_iso benchmark::benchmark_main phantasm_warnings
)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Benchmarks are built, but numbers are only meaningful with --preset release")
endif()
