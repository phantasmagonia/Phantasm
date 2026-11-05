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

    input.begin_frame();
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

using phantasm::MouseButton;
using phantasm::Vec2;

TEST_CASE("Mouse position is tracked with a per-frame delta") {
    Input input;
    input.set_mouse_position(Vec2{10, 20});
    input.begin_frame();
    input.set_mouse_position(Vec2{15, 18});

    CHECK(input.mouse_position().x == 15.0F);
    CHECK(input.mouse_position().y == 18.0F);
    CHECK(input.mouse_delta().x == 5.0F);
    CHECK(input.mouse_delta().y == -2.0F);

    input.begin_frame();
    CHECK(input.mouse_delta().x == 0.0F);
}

TEST_CASE("A mouse button is pressed on the frame it goes down") {
    Input input;
    input.begin_frame();
    input.set_mouse_button(MouseButton::Left, true);
    CHECK(input.is_down(MouseButton::Left));
    CHECK(input.was_pressed(MouseButton::Left));
    CHECK_FALSE(input.is_down(MouseButton::Right));

    input.begin_frame();
    CHECK(input.is_down(MouseButton::Left));
    CHECK_FALSE(input.was_pressed(MouseButton::Left));

    input.set_mouse_button(MouseButton::Left, false);
    CHECK(input.was_released(MouseButton::Left));
}

TEST_CASE("A click that goes down and up within one frame is not lost") {
    Input input;
    input.begin_frame();
    input.set_mouse_button(MouseButton::Right, true);
    input.set_mouse_button(MouseButton::Right, false);

    CHECK(input.was_pressed(MouseButton::Right));
    CHECK(input.was_released(MouseButton::Right));
    CHECK_FALSE(input.is_down(MouseButton::Right));
}

TEST_CASE("Repeated down events while held are not new presses") {
    Input input;
    input.begin_frame();
    input.set_mouse_button(MouseButton::Left, true);
    input.begin_frame();
    input.set_mouse_button(MouseButton::Left, true);

    CHECK_FALSE(input.was_pressed(MouseButton::Left));
}

TEST_CASE("Mouse wheel movement accumulates within a frame and then resets") {
    Input input;
    input.begin_frame();
    input.add_mouse_wheel(1.0F);
    input.add_mouse_wheel(0.5F);
    CHECK(input.mouse_wheel() == 1.5F);

    input.begin_frame();
    CHECK(input.mouse_wheel() == 0.0F);
}
