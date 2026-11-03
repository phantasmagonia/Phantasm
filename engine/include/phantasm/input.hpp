#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>

namespace phantasm {

// Physical key positions. Values are USB HID usage IDs, which SDL scancodes
// also use, so the platform layer converts with a range-checked cast.
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

// Keyboard state for the current frame. Pure logic, no SDL.
// Edge queries (pressed/released) compare this frame against the previous one,
// so a press AND release inside a single frame is not seen. Fine at 60+ FPS.
class Input {
public:
    // Call once at the start of each frame, before feeding new events.
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
