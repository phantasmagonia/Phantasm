#include <phantasm/input.hpp>

namespace phantasm {

namespace {
constexpr std::size_t index_of(Key key) noexcept {
    return static_cast<std::size_t>(key);
}
} // namespace

void Input::begin_frame() noexcept {
    previous_ = current_;
    previous_mouse_ = mouse_;
    wheel_ = 0.0F;
    buttons_pressed_.fill(false);
    buttons_released_.fill(false);
}

void Input::set_key(Key key, bool down) noexcept {
    current_.set(index_of(key), down);
}

bool Input::is_down(Key key) const noexcept {
    return current_.test(index_of(key));
}

bool Input::was_pressed(Key key) const noexcept {
    return current_.test(index_of(key)) && !previous_.test(index_of(key));
}

bool Input::was_released(Key key) const noexcept {
    return !current_.test(index_of(key)) && previous_.test(index_of(key));
}

void Input::set_mouse_position(Vec2 position) noexcept {
    mouse_ = position;
}

void Input::set_mouse_button(MouseButton button, bool down) noexcept {
    const auto index = static_cast<std::size_t>(button);
    if (index >= kMouseButtonCount) {
        return;
    }
    if (down && !buttons_down_[index]) {
        buttons_pressed_[index] = true;
    } else if (!down && buttons_down_[index]) {
        buttons_released_[index] = true;
    }
    buttons_down_[index] = down;
}

void Input::add_mouse_wheel(float amount) noexcept {
    wheel_ += amount;
}

Vec2 Input::mouse_delta() const noexcept {
    return Vec2{mouse_.x - previous_mouse_.x, mouse_.y - previous_mouse_.y};
}

bool Input::is_down(MouseButton button) const noexcept {
    const auto index = static_cast<std::size_t>(button);
    return index < kMouseButtonCount && buttons_down_[index];
}

bool Input::was_pressed(MouseButton button) const noexcept {
    const auto index = static_cast<std::size_t>(button);
    return index < kMouseButtonCount && buttons_pressed_[index];
}

bool Input::was_released(MouseButton button) const noexcept {
    const auto index = static_cast<std::size_t>(button);
    return index < kMouseButtonCount && buttons_released_[index];
}

} // namespace phantasm
