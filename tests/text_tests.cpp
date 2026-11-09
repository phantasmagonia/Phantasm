#include <phantasm/text.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using phantasm::FontFace;
using phantasm::decode_utf8;

namespace {

// Tests use a font installed on the machine; no font files ship in this repository.
std::optional<std::vector<std::byte>> system_font() {
    const std::array candidates{
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    for (const char* path : candidates) {
        std::ifstream file{path, std::ios::binary};
        if (!file) {
            continue;
        }
        const std::vector<char> chars{std::istreambuf_iterator<char>{file}, {}};
        std::vector<std::byte> bytes(chars.size());
        for (std::size_t i = 0; i < chars.size(); ++i) {
            bytes[i] = static_cast<std::byte>(chars[i]);
        }
        return bytes;
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("UTF-8 decodes ASCII and multi-byte characters") {
    CHECK(decode_utf8("Az") == std::u32string{U'A', U'z'});
    CHECK(decode_utf8("\xC4\x9F\xC5\x9F") == std::u32string{U'\u011F', U'\u015F'}); // ğ ş
    CHECK(decode_utf8("\xE2\x82\xAC") == std::u32string{U'\u20AC'});               // €
    CHECK(decode_utf8("\xF0\x9F\x98\x80") == std::u32string{U'\U0001F600'});        // emoji
}

TEST_CASE("Invalid UTF-8 becomes the replacement character instead of failing") {
    CHECK(decode_utf8("\xFF") == std::u32string{U'\uFFFD'});
    CHECK(decode_utf8("\xC4") == std::u32string{U'\uFFFD'});         // truncated sequence
    CHECK(decode_utf8("a\x80" "b") == std::u32string{U'a', U'\uFFFD', U'b'}); // stray continuation
    CHECK(decode_utf8("\xC0\xAF") == std::u32string{U'\uFFFD', U'\uFFFD'});  // overlong encoding
}

TEST_CASE("Invalid font data is rejected") {
    const std::vector<std::byte> junk(64, std::byte{0x42});
    CHECK_THROWS_AS(FontFace::parse(junk, 32.0F), std::runtime_error);
}

TEST_CASE("A font face exposes metrics at the requested pixel height") {
    const auto font = system_font();
    if (!font) {
        SKIP("No system font found");
    }
    const FontFace face = FontFace::parse(*font, 32.0F);

    CHECK(face.pixel_height() == 32.0F);
    CHECK(face.ascent() > 0.0F);
    CHECK(face.descent() <= 0.0F);
    CHECK(face.line_height() >= face.ascent() - face.descent());
    CHECK(face.atlas_width() > 0);
    CHECK(face.atlas_pixels().size() ==
          static_cast<std::size_t>(face.atlas_width() * face.atlas_height() * 4));
}

TEST_CASE("Layout places glyphs left to right and wider text measures wider") {
    const auto font = system_font();
    if (!font) {
        SKIP("No system font found");
    }
    const FontFace face = FontFace::parse(*font, 32.0F);

    const auto layout = face.layout("Hi");
    REQUIRE(layout.glyphs.size() == 2);
    CHECK(layout.glyphs[1].rect.x > layout.glyphs[0].rect.x);
    CHECK(face.measure("Hello").x > face.measure("Hi").x);
    CHECK(face.measure("").x == 0.0F);
}

TEST_CASE("Spaces advance the pen but produce no glyph quad") {
    const auto font = system_font();
    if (!font) {
        SKIP("No system font found");
    }
    const FontFace face = FontFace::parse(*font, 32.0F);

    const auto layout = face.layout("a b");

    CHECK(layout.glyphs.size() == 2);
    CHECK(face.measure("a b").x > face.measure("ab").x);
}

TEST_CASE("Newlines start a new line one line height lower") {
    const auto font = system_font();
    if (!font) {
        SKIP("No system font found");
    }
    const FontFace face = FontFace::parse(*font, 32.0F);

    const auto layout = face.layout("A\nA");

    REQUIRE(layout.glyphs.size() == 2);
    CHECK(layout.glyphs[0].rect.x == Catch::Approx(layout.glyphs[1].rect.x));
    CHECK(layout.glyphs[1].rect.y - layout.glyphs[0].rect.y == Catch::Approx(face.line_height()));
    CHECK(face.measure("A\nA").y == Catch::Approx(2.0F * face.line_height()));
}

TEST_CASE("Kerning pulls some letter pairs closer together") {
    const auto font = system_font();
    if (!font) {
        SKIP("No system font found");
    }
    const FontFace face = FontFace::parse(*font, 64.0F);

    // Layout applies exactly the font's kerning, whatever the installed font provides.
    const float kerning = face.kerning(U'A', U'V');
    CHECK(face.measure("AV").x ==
          Catch::Approx(face.measure("A").x + face.measure("V").x + kerning).margin(0.01F));
    if (kerning < 0.0F) {
        // "AV" is the classic kerned pair: with kerning data, it is narrower than apart.
        CHECK(face.measure("AV").x < face.measure("A").x + face.measure("V").x);
    }
}

TEST_CASE("Kerning is zero for characters that are not baked") {
    const auto font = system_font();
    if (!font) {
        SKIP("No system font found");
    }
    const FontFace face = FontFace::parse(*font, 64.0F, phantasm::FontOptions{.characters = U"A"});

    CHECK(face.kerning(U'A', U'V') == 0.0F);
}

TEST_CASE("Turkish letters are in the default character set") {
    const auto font = system_font();
    if (!font) {
        SKIP("No system font found");
    }
    const FontFace face = FontFace::parse(*font, 32.0F);

    CHECK(face.has_glyph(U'\u011F')); // ğ
    CHECK(face.has_glyph(U'\u0131')); // ı
    CHECK(face.has_glyph(U'\u0130')); // İ
    CHECK_FALSE(face.has_glyph(U'\u4E2D')); // CJK is not baked by default
}

TEST_CASE("Characters missing from the atlas fall back to a question mark") {
    const auto font = system_font();
    if (!font) {
        SKIP("No system font found");
    }
    const FontFace face = FontFace::parse(*font, 32.0F);

    const auto missing = face.layout("\xE4\xB8\xAD"); // a CJK character, not baked
    const auto question = face.layout("?");

    REQUIRE(missing.glyphs.size() == 1);
    REQUIRE(question.glyphs.size() == 1);
    CHECK(missing.glyphs[0].uv.x == question.glyphs[0].uv.x);
    CHECK(missing.glyphs[0].uv.y == question.glyphs[0].uv.y);
}

TEST_CASE("Only the requested characters are baked when a character set is given") {
    const auto font = system_font();
    if (!font) {
        SKIP("No system font found");
    }
    const FontFace face = FontFace::parse(*font, 32.0F, phantasm::FontOptions{.characters = U"AB?"});

    CHECK(face.has_glyph(U'A'));
    CHECK(face.has_glyph(U'B'));
    CHECK_FALSE(face.has_glyph(U'C'));
}

TEST_CASE("A missing character is skipped when the font has no question mark either") {
    const auto font = system_font();
    if (!font) {
        SKIP("No system font found");
    }
    const FontFace face = FontFace::parse(*font, 32.0F, phantasm::FontOptions{.characters = U"AB"});

    const auto layout = face.layout("ACB");

    CHECK(layout.glyphs.size() == 2);
    CHECK(face.measure("ACB").x == face.measure("AB").x);
}

TEST_CASE("A truncated font file is rejected before it is parsed") {
    const auto font = system_font();
    if (!font) {
        SKIP("No system font found");
    }
    const std::vector<std::byte> truncated(font->begin(), font->begin() + 512);

    CHECK_THROWS_AS(FontFace::parse(truncated, 32.0F), std::runtime_error);
}

TEST_CASE("A font whose table directory points outside the file is rejected") {
    // TrueType signature, then a claim of 200 tables in a 64-byte file.
    std::vector<std::byte> bogus(64, std::byte{0});
    bogus[1] = std::byte{0x01};
    bogus[5] = std::byte{200};

    CHECK_THROWS_AS(FontFace::parse(bogus, 32.0F), std::runtime_error);
}
