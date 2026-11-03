add_executable(phantasm_tests
    camera_tests.cpp
    frame_clock_tests.cpp
    input_tests.cpp
    log_tests.cpp
    sprite_batch_tests.cpp
    sprite_tests.cpp
)
target_link_libraries(phantasm_tests PRIVATE phantasm Catch2::Catch2WithMain phantasm_warnings)

include(Catch)
catch_discover_tests(phantasm_tests)
