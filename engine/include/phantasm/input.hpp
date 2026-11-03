#pragma once

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

// A press and release within one frame is not detected.
class Input {
public:
    void begin_frame() noexcept;

    void set_key(Key key, bool down) noexcept;

    [[nodiscard]] bool is_down(Key key) const noexcept;
    [[nodiscard]] bool was_pressed(Key key) const noexcept;
    [[nodiscard]] bool was_released(Key key) const noexcept;

private:
    std::bitset<kKeyCount> current_;
    std::bitset<kKeyCount> previous_;
};

} // namespace phantasm
