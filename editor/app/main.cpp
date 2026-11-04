#include <phantasm/editor/editor.hpp>

#include <cstdio>
#include <exception>
#include <filesystem>
#include <optional>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc > 2) {
            throw std::invalid_argument("Usage: phantasm_editor [scene.phscene]");
        }
        phantasm::editor::Editor editor{{
            .scene = argc == 2 ? std::optional<std::filesystem::path>{argv[1]} : std::nullopt,
        }};
        editor.run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Phantasm Editor: %s\n", error.what());
        return 1;
    }
    return 0;
}
