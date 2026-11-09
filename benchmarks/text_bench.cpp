#include <phantasm/text.hpp>

#include <benchmark/benchmark.h>

#include <array>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace {

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

// A HUD-sized paragraph: mixed ASCII, punctuation, Turkish letters, and line breaks.
std::string sample_text(std::size_t repeats) {
    std::string text;
    for (std::size_t i = 0; i < repeats; ++i) {
        text += "Radiation 42% - Ammo 7/30. Kapı kilitli; anahtar gerekli.\n";
    }
    return text;
}

void BM_TextLayout(benchmark::State& state) {
    const auto font = system_font();
    if (!font) {
        state.SkipWithError("No system font found");
        return;
    }
    const auto face = phantasm::FontFace::parse(*font, 24.0F);
    const std::string text = sample_text(static_cast<std::size_t>(state.range(0)));
    for (auto _ : state) {
        benchmark::DoNotOptimize(face.layout(text));
    }
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(text.size()));
}

} // namespace

BENCHMARK(BM_TextLayout)->Arg(1)->Arg(100)->Unit(benchmark::kMicrosecond);
