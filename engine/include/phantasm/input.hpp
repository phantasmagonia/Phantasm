#pragma once

#include <phantasm/geometry.hpp>

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>

namespace phantasm {

// Values are USB HID usage IDs, identical to SDL scancodes.
enum class Key : std::uint16_t {
    A = 4, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num1 = 30, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9, Num0,
    Enter = 40,
    Escape = 41,
    Backspace = 42,
    Tab = 43,
    Space = 44,
    Right = 79,
    Left = 80,
    Down = 81,
    Up = 82,
};

inline constexpr std::size_t kKeyCount = 512;

enum class MouseButton : std::uint8_t { Left, Middle, Right, X1, X2 };
inline constexpr std::size_t kMouseButtonCount = 5;

// Keyboard: a key pressed and released within one frame is not detected.
// Mouse buttons latch press/release events, so short clicks are never lost.
class Input {
public:
    void begin_frame() noexcept;

    void set_key(Key key, bool down) noexcept;

    [[nodiscard]] bool is_down(Key key) const noexcept;
    [[nodiscard]] bool was_pressed(Key key) const noexcept;
    [[nodiscard]] bool was_released(Key key) const noexcept;

    // Window coordinates in pixels, origin top-left.
    void set_mouse_position(Vec2 position) noexcept;
    void set_mouse_button(MouseButton button, bool down) noexcept;
    void add_mouse_wheel(float amount) noexcept; // positive = away from the user

    [[nodiscard]] Vec2 mouse_position() const noexcept { return mouse_; }
    [[nodiscard]] Vec2 mouse_delta() const noexcept;
    [[nodiscard]] float mouse_wheel() const noexcept { return wheel_; }
    [[nodiscard]] bool is_down(MouseButton button) const noexcept;
    [[nodiscard]] bool was_pressed(MouseButton button) const noexcept;
    [[nodiscard]] bool was_released(MouseButton button) const noexcept;

private:
    std::bitset<kKeyCount> current_;
    std::bitset<kKeyCount> previous_;
    Vec2 mouse_{};
    Vec2 previous_mouse_{};
    float wheel_ = 0.0F;
    std::array<bool, kMouseButtonCount> buttons_down_{};
    std::array<bool, kMouseButtonCount> buttons_pressed_{};
    std::array<bool, kMouseButtonCount> buttons_released_{};
};

} // namespace phantasm
