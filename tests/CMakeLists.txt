add_executable(phantasm_tests
    async_chunk_stream_tests.cpp
    atlas_tests.cpp
    camera_tests.cpp
    chunk_index_tests.cpp
    chunk_stream_tests.cpp
    fixed_step_tests.cpp
    frame_clock_tests.cpp
    frame_stats_tests.cpp
    geometry_tests.cpp
    input_tests.cpp
    iso_grid_tests.cpp
    log_tests.cpp
    sprite_batch_tests.cpp
    sort_key_tests.cpp
    task_pool_tests.cpp
    sprite_tests.cpp
    tile_map_tests.cpp
)
target_link_libraries(phantasm_tests PRIVATE phantasm_iso Catch2::Catch2WithMain phantasm_warnings)

if(PHANTASM_BUILD_PHYSICS)
    target_sources(phantasm_tests PRIVATE physics_world_tests.cpp)
    target_link_libraries(phantasm_tests PRIVATE phantasm_physics)
endif()

include(Catch)
catch_discover_tests(phantasm_tests)
