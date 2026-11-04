#pragma once

#include <phantasm/iso/scene.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <tuple>
#include <variant>
#include <vector>

namespace phantasm::iso {

// One undo step per tile stroke or entity action. History is editor state, not saved to disk.
class SceneHistory {
public:
    explicit SceneHistory(Scene& scene) : scene_{scene} {}

    void paint(TileCoord tile, char value);
    void end_stroke();
    [[nodiscard]] std::uint64_t create(GridPos position, std::string name,
                                       std::string asset = {}, float scale = 1.0F);
    [[nodiscard]] bool erase(std::uint64_t id);
    void record_object_change(const SceneObject& before);
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] bool can_undo() const noexcept { return !undo_.empty() || !stroke_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_.empty(); }
    void clear() noexcept;

private:
    struct TileChange {
        TileCoord tile;
        char before;
        char after;
    };
    using TileStroke = std::vector<TileChange>;
    struct ObjectChange {
        std::optional<SceneObject> before;
        std::optional<SceneObject> after;
    };
    using Command = std::variant<TileStroke, ObjectChange>;

    void record(Command command);
    void apply(const Command& command, bool forward);

    Scene& scene_;
    TileStroke stroke_;
    std::map<std::tuple<int, int, int>, std::size_t> stroke_lookup_;
    std::vector<Command> undo_;
    std::vector<Command> redo_;
};

} // namespace phantasm::iso
