add_executable(phantasm_tests
    atlas_tests.cpp
    camera_tests.cpp
    frame_clock_tests.cpp
    frame_stats_tests.cpp
    geometry_tests.cpp
    input_tests.cpp
    iso_grid_tests.cpp
    log_tests.cpp
    sprite_batch_tests.cpp
    sort_key_tests.cpp
    sprite_tests.cpp
    tile_map_tests.cpp
)
target_link_libraries(phantasm_tests PRIVATE phantasm_iso Catch2::Catch2WithMain phantasm_warnings)

include(Catch)
catch_discover_tests(phantasm_tests)
