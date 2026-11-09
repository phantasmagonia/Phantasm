#pragma once

#include <phantasm/geometry.hpp>

#include <cstddef>
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace phantasm {

[[nodiscard]] std::u32string decode_utf8(std::string_view text);

// Characters baked into the font atlas. Empty means the default set: printable ASCII,
// Latin-1, and Latin Extended-A (covers Western European and Turkish text).
struct FontOptions {
    std::u32string characters{};
};

struct TextGlyph {
    Rect rect{};
    Rect uv{};
};

struct TextLayout {
    std::vector<TextGlyph> glyphs;
    Vec2 size{};
};

// Parsed font plus a baked glyph atlas (RGBA8, white with coverage in alpha). Load only fonts
// you ship or trust: structure is validated, but glyph data is not fully sanitized.
class FontFace {
public:
    // Throws std::runtime_error for invalid font data or a non-positive / too large height.
    [[nodiscard]] static FontFace parse(std::span<const std::byte> bytes, float pixel_height,
                                        const FontOptions& options = {});

    [[nodiscard]] float pixel_height() const noexcept { return pixel_height_; }
    [[nodiscard]] float ascent() const noexcept { return ascent_; }
    [[nodiscard]] float descent() const noexcept { return descent_; }
    [[nodiscard]] float line_height() const noexcept { return line_height_; }
    [[nodiscard]] int atlas_width() const noexcept { return atlas_width_; }
    [[nodiscard]] int atlas_height() const noexcept { return atlas_height_; }
    [[nodiscard]] std::span<const std::byte> atlas_pixels() const noexcept { return atlas_pixels_; }
    [[nodiscard]] bool has_glyph(char32_t codepoint) const noexcept;
    // Horizontal adjustment between two baked characters, in pixels (0 if none).
    [[nodiscard]] float kerning(char32_t left, char32_t right) const noexcept;
    [[nodiscard]] TextLayout layout(std::string_view text) const;
    [[nodiscard]] Vec2 measure(std::string_view text) const { return layout(text).size; }

private:
    struct Glyph {
        char32_t codepoint{};
        int font_index = 0; // glyph id inside the font, for kerning
        Rect rect{};
        Rect uv{};
        float advance{};
    };
    static constexpr char32_t kDirectLookup = 0x180; // ASCII through Latin Extended-A
    static constexpr std::uint16_t kNoGlyph = 0xFFFF;

    FontFace();
    // The glyph, else the '?' glyph, else nullptr.
    [[nodiscard]] const Glyph* find_glyph(char32_t codepoint) const noexcept;

    std::vector<std::byte> font_bytes_;
    std::vector<std::byte> atlas_pixels_;
    std::vector<Glyph> glyphs_;
    std::array<std::uint16_t, kDirectLookup> direct_{}; // codepoint -> index into glyphs_
    std::shared_ptr<const void> font_info_;              // stbtt_fontinfo over font_bytes_
    float pixel_height_{};
    float ascent_{};
    float descent_{};
    float line_height_{};
    float scale_{};
    int atlas_width_ = 1024;
    int atlas_height_ = 1024;
};

} // namespace phantasm
