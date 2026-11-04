add_executable(phantasm_benchmarks
    chunk_index_bench.cpp
    entity_store_bench.cpp
    iso_bench.cpp
    iso_prepare_bench.cpp
    sprite_batch_bench.cpp
    world_update_bench.cpp
)
target_link_libraries(phantasm_benchmarks
    PRIVATE phantasm_iso benchmark::benchmark_main phantasm_warnings
)

if(PHANTASM_BUILD_PHYSICS)
    target_sources(phantasm_benchmarks PRIVATE physics_bench.cpp)
    target_link_libraries(phantasm_benchmarks PRIVATE phantasm_physics)
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Benchmarks are built, but numbers are only meaningful with --preset release")
endif()
