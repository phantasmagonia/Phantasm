#include <phantasm/input.hpp>

namespace phantasm {

namespace {
constexpr std::size_t index_of(Key key) noexcept {
    return static_cast<std::size_t>(key);
}
} // namespace

void Input::begin_frame() noexcept {
    previous_ = current_;
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

} // namespace phantasm
