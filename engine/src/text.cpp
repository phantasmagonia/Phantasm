#include <phantasm/text.hpp>
#include <phantasm/renderer.hpp>

#include <stb_truetype.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <stdexcept>
#include <memory>
#include <vector>

namespace phantasm {

namespace {

// Printable ASCII, Latin-1, and Latin Extended-A (Western European and Turkish text).
std::u32string default_characters() {
    std::u32string characters;
    for (char32_t cp = 0x20; cp <= 0x17F; ++cp) {
        if (cp < 0x7F || cp >= 0xA0) {
            characters.push_back(cp);
        }
    }
    return characters;
}

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t at) noexcept {
    return (std::to_integer<std::uint32_t>(bytes[at]) << 24) |
           (std::to_integer<std::uint32_t>(bytes[at + 1]) << 16) |
           (std::to_integer<std::uint32_t>(bytes[at + 2]) << 8) |
           std::to_integer<std::uint32_t>(bytes[at + 3]);
}

std::uint16_t read_u16(std::span<const std::byte> bytes, std::size_t at) noexcept {
    return static_cast<std::uint16_t>((std::to_integer<std::uint32_t>(bytes[at]) << 8) |
                                      std::to_integer<std::uint32_t>(bytes[at + 1]));
}

constexpr std::uint32_t tag(const char (&name)[5]) noexcept {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(name[0])) << 24) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(name[1])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(name[2])) << 8) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(name[3]));
}

// stb_truetype does no bounds checking, so reject files whose structure does not fit
// before it reads them. Table contents are still trusted: load fonts you ship.
bool font_structure_is_valid(std::span<const std::byte> bytes) noexcept {
    constexpr std::size_t kHeaderSize = 12;
    constexpr std::size_t kRecordSize = 16;
    if (bytes.size() < kHeaderSize) {
        return false;
    }
    std::size_t font_offset = 0;
    if (read_u32(bytes, 0) == tag("ttcf")) {
        if (bytes.size() < 16 || read_u32(bytes, 8) == 0) {
            return false;
        }
        font_offset = read_u32(bytes, 12); // first font of the collection
        if (font_offset > bytes.size() - kHeaderSize) {
            return false;
        }
    }
    const std::uint32_t version = read_u32(bytes, font_offset);
    if (version != 0x00010000U && version != tag("true") && version != tag("OTTO")) {
        return false;
    }
    const std::size_t table_count = read_u16(bytes, font_offset + 4);
    const std::size_t directory_end = font_offset + kHeaderSize + table_count * kRecordSize;
    if (directory_end > bytes.size()) {
        return false;
    }
    bool cmap = false, head = false, hhea = false, hmtx = false, glyf = false, loca = false,
         cff = false;
    for (std::size_t i = 0; i < table_count; ++i) {
        const std::size_t record = font_offset + kHeaderSize + i * kRecordSize;
        const std::uint64_t offset = read_u32(bytes, record + 8);
        const std::uint64_t length = read_u32(bytes, record + 12);
        if (offset + length > bytes.size()) {
            return false;
        }
        const std::uint32_t name = read_u32(bytes, record);
        cmap = cmap || name == tag("cmap");
        head = head || name == tag("head");
        hhea = hhea || name == tag("hhea");
        hmtx = hmtx || name == tag("hmtx");
        glyf = glyf || name == tag("glyf");
        loca = loca || name == tag("loca");
        cff = cff || name == tag("CFF ");
    }
    return cmap && head && hhea && hmtx && ((glyf && loca) || cff);
}

} // namespace

std::u32string decode_utf8(std::string_view text) {
    std::u32string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        const int count = lead < 0x80 ? 1 : lead >= 0xC2 && lead <= 0xDF ? 2
                            : lead >= 0xE0 && lead <= 0xEF ? 3
                            : lead >= 0xF0 && lead <= 0xF4 ? 4 : 0;
        if (count == 0 || i + static_cast<std::size_t>(count) > text.size()) {
            result.push_back(U'\uFFFD');
            ++i;
            continue;
        }
        char32_t value = lead & (count == 1 ? 0x7F : count == 2 ? 0x1F : count == 3 ? 0x0F : 0x07);
        bool valid = true;
        for (int j = 1; j < count; ++j) {
            const auto byte = static_cast<unsigned char>(text[i + static_cast<std::size_t>(j)]);
            if ((byte & 0xC0) != 0x80) { valid = false; break; }
            value = (value << 6) | (byte & 0x3F);
        }
        if (!valid || (count == 2 && value < 0x80) || (count == 3 && value < 0x800) ||
            (count == 4 && value < 0x10000) || (value >= 0xD800 && value <= 0xDFFF) ||
            value > 0x10FFFF) {
            result.push_back(U'\uFFFD');
            ++i;
        } else {
            result.push_back(value);
            i += static_cast<std::size_t>(count);
        }
    }
    return result;
}

FontFace::FontFace() {
    direct_.fill(kNoGlyph);
}

FontFace FontFace::parse(std::span<const std::byte> bytes, float pixel_height,
                         const FontOptions& options) {
    if (bytes.empty() || bytes.size() > static_cast<std::size_t>(INT_MAX) ||
        !std::isfinite(pixel_height) || pixel_height <= 0.0F || pixel_height > 256.0F) {
        throw std::runtime_error("Invalid font data or pixel height");
    }
    if (!font_structure_is_valid(bytes)) {
        throw std::runtime_error("Invalid or truncated font file");
    }
    FontFace face;
    face.font_bytes_.assign(bytes.begin(), bytes.end());
    const auto* data = reinterpret_cast<const unsigned char*>(face.font_bytes_.data());
    auto info = std::make_shared<stbtt_fontinfo>();
    if (stbtt_InitFont(info.get(), data, stbtt_GetFontOffsetForIndex(data, 0)) == 0) {
        throw std::runtime_error("Invalid TrueType font");
    }
    const stbtt_fontinfo& font = *info;
    face.font_info_ = info; // points into font_bytes_, whose heap buffer survives moves
    face.pixel_height_ = pixel_height;
    face.scale_ = stbtt_ScaleForPixelHeight(&font, pixel_height);
    int ascent = 0, descent = 0, gap = 0;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &gap);
    face.ascent_ = ascent * face.scale_;
    face.descent_ = descent * face.scale_;
    face.line_height_ = (ascent - descent + gap) * face.scale_;
    face.atlas_pixels_.resize(static_cast<std::size_t>(face.atlas_width_ * face.atlas_height_ * 4), std::byte{0});

    const std::u32string chars = options.characters.empty() ? default_characters()
                                                            : options.characters;
    int x = 1, y = 1, row_height = 0;
    for (char32_t cp : chars) {
        const int font_index = stbtt_FindGlyphIndex(&font, static_cast<int>(cp));
        if (font_index == 0) continue;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetCodepointBitmapBox(&font, static_cast<int>(cp), face.scale_, face.scale_, &x0, &y0, &x1, &y1);
        const int w = x1 - x0, h = y1 - y0;
        if (x + w + 1 >= face.atlas_width_) { x = 1; y += row_height + 1; row_height = 0; }
        if (y + h + 1 >= face.atlas_height_) throw std::runtime_error("Font atlas is full");
        int advance = 0, bearing = 0;
        stbtt_GetCodepointHMetrics(&font, static_cast<int>(cp), &advance, &bearing);
        (void)bearing;
        if (w > 0 && h > 0) {
            std::vector<unsigned char> bitmap(static_cast<std::size_t>(w * h));
            stbtt_MakeCodepointBitmap(&font, bitmap.data(), w, h, w, face.scale_, face.scale_, static_cast<int>(cp));
            for (int py = 0; py < h; ++py) for (int px = 0; px < w; ++px) {
                const auto dst = static_cast<std::size_t>(((y + py) * face.atlas_width_ + x + px) * 4);
                face.atlas_pixels_[dst] = face.atlas_pixels_[dst + 1] = face.atlas_pixels_[dst + 2] = std::byte{255};
                face.atlas_pixels_[dst + 3] = static_cast<std::byte>(bitmap[static_cast<std::size_t>(py * w + px)]);
            }
        }
        if (cp < kDirectLookup) {
            face.direct_[cp] = static_cast<std::uint16_t>(face.glyphs_.size());
        }
        face.glyphs_.push_back({cp, font_index, {static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(w), static_cast<float>(h)},
            {static_cast<float>(x) / face.atlas_width_, static_cast<float>(y) / face.atlas_height_,
             static_cast<float>(w) / face.atlas_width_, static_cast<float>(h) / face.atlas_height_}, advance * face.scale_});
        x += w + 1;
        row_height = std::max(row_height, h);
    }
    return face;
}

bool FontFace::has_glyph(char32_t codepoint) const noexcept {
    if (codepoint < kDirectLookup) {
        return direct_[codepoint] != kNoGlyph;
    }
    return std::any_of(glyphs_.begin(), glyphs_.end(),
                       [codepoint](const Glyph& glyph) { return glyph.codepoint == codepoint; });
}

float FontFace::kerning(char32_t left, char32_t right) const noexcept {
    if (!has_glyph(left) || !has_glyph(right)) {
        return 0.0F;
    }
    const auto& font = *static_cast<const stbtt_fontinfo*>(font_info_.get());
    return static_cast<float>(stbtt_GetGlyphKernAdvance(&font, find_glyph(left)->font_index,
                                                        find_glyph(right)->font_index)) *
           scale_;
}

const FontFace::Glyph* FontFace::find_glyph(char32_t codepoint) const noexcept {
    if (codepoint < kDirectLookup && direct_[codepoint] != kNoGlyph) {
        return &glyphs_[direct_[codepoint]];
    }
    const auto matches = [](char32_t wanted) {
        return [wanted](const Glyph& glyph) { return glyph.codepoint == wanted; };
    };
    auto it = std::find_if(glyphs_.begin(), glyphs_.end(), matches(codepoint));
    if (it == glyphs_.end()) {
        it = std::find_if(glyphs_.begin(), glyphs_.end(), matches(U'?'));
    }
    return it == glyphs_.end() ? nullptr : &*it;
}

TextLayout FontFace::layout(std::string_view text) const {
    TextLayout result;
    if (text.empty()) return result;
    const auto& font = *static_cast<const stbtt_fontinfo*>(font_info_.get());
    float pen_x = 0.0F, pen_y = ascent_, max_width = 0.0F;
    int previous = 0; // font glyph index of the previous character, 0 = none
    for (char32_t cp : decode_utf8(text)) {
        if (cp == U'\n') {
            max_width = std::max(max_width, pen_x);
            pen_x = 0.0F;
            pen_y += line_height_;
            previous = 0;
            continue;
        }
        if (cp == U'\r') continue;
        const Glyph* glyph = find_glyph(cp);
        if (glyph == nullptr) {
            continue; // not baked, and the font has no '?' to stand in
        }
        if (previous != 0) {
            pen_x += static_cast<float>(stbtt_GetGlyphKernAdvance(&font, previous, glyph->font_index)) * scale_;
        }
        if (glyph->rect.w > 0.0F && glyph->rect.h > 0.0F)
            result.glyphs.push_back({{pen_x + glyph->rect.x, pen_y + glyph->rect.y, glyph->rect.w, glyph->rect.h}, glyph->uv});
        pen_x += glyph->advance;
        previous = glyph->font_index;
    }
    result.size = {std::max(max_width, pen_x), pen_y - ascent_ + line_height_};
    return result;
}

void Renderer::draw_text(const Texture& atlas, const FontFace& face, std::string_view text,
                         Vec2 position, Color color, int layer) {
    draw_text_layout(atlas, face.layout(text), position, color, layer);
}

void Renderer::draw_text_layout(const Texture& atlas, const TextLayout& layout, Vec2 position,
                                Color color, int layer) {
    for (const TextGlyph& glyph : layout.glyphs) {
        Sprite sprite;
        sprite.position = {position.x + glyph.rect.x, position.y + glyph.rect.y};
        sprite.size = {glyph.rect.w, glyph.rect.h};
        sprite.uv = glyph.uv;
        sprite.tint = color;
        sprite.layer = layer;
        draw_sprite(atlas, sprite);
    }
}

} // namespace phantasm
