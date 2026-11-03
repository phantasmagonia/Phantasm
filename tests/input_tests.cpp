#include <phantasm/input.hpp>

#include <catch2/catch_test_macros.hpp>

using phantasm::Input;
using phantasm::Key;

TEST_CASE("A key is pressed only on the frame it goes down") {
    Input input;

    input.begin_frame();
    input.set_key(Key::Space, true);
    CHECK(input.is_down(Key::Space));
    CHECK(input.was_pressed(Key::Space));

    input.begin_frame(); // still held
    CHECK(input.is_down(Key::Space));
    CHECK_FALSE(input.was_pressed(Key::Space));
}

TEST_CASE("A key is released only on the frame it goes up") {
    Input input;
    input.set_key(Key::A, true);

    input.begin_frame();
    input.set_key(Key::A, false);
    CHECK_FALSE(input.is_down(Key::A));
    CHECK(input.was_released(Key::A));

    input.begin_frame();
    CHECK_FALSE(input.was_released(Key::A));
}

TEST_CASE("Keys are tracked independently") {
    Input input;
    input.begin_frame();
    input.set_key(Key::Left, true);

    CHECK(input.is_down(Key::Left));
    CHECK_FALSE(input.is_down(Key::Right));
}

TEST_CASE("Repeated key-down events while held do not re-trigger a press") {
    Input input;
    input.begin_frame();
    input.set_key(Key::Up, true);

    input.begin_frame();
    input.set_key(Key::Up, true); // OS key-repeat
    CHECK_FALSE(input.was_pressed(Key::Up));
}
