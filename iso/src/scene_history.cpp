#include <phantasm/iso/scene_history.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace phantasm::iso {
namespace {

bool same_object(const SceneObject& a, const SceneObject& b) {
    return a.id == b.id && a.position.x == b.position.x && a.position.y == b.position.y &&
           a.position.level == b.position.level && a.name == b.name && a.asset == b.asset &&
           a.scale == b.scale;
}

} // namespace

void SceneHistory::paint(TileCoord tile, char value) {
    const char before = scene_.map().at(tile);
    if (before == value) return;
    scene_.map().set(tile, value);
    const auto key = std::tuple{tile.level, tile.y, tile.x};
    const auto [it, inserted] = stroke_lookup_.try_emplace(key, stroke_.size());
    if (inserted) {
        stroke_.push_back(TileChange{tile, before, value});
    } else {
        stroke_[it->second].after = value;
    }
}

void SceneHistory::record(Command command) {
    undo_.push_back(std::move(command));
    redo_.clear();
}

void SceneHistory::end_stroke() {
    if (stroke_.empty()) return;
    std::erase_if(stroke_, [](const TileChange& change) { return change.before == change.after; });
    if (!stroke_.empty()) record(std::move(stroke_));
    stroke_.clear();
    stroke_lookup_.clear();
}

std::uint64_t SceneHistory::create(GridPos position, std::string name,
                                    std::string asset, float scale) {
    end_stroke();
    const auto id = scene_.create(position, std::move(name), std::move(asset), scale);
    record(ObjectChange{std::nullopt, *scene_.object(id)});
    return id;
}

bool SceneHistory::erase(std::uint64_t id) {
    end_stroke();
    const SceneObject* object = scene_.object(id);
    if (object == nullptr) return false;
    const SceneObject before = *object;
    (void)scene_.erase(id);
    record(ObjectChange{before, std::nullopt});
    return true;
}

void SceneHistory::record_object_change(const SceneObject& before) {
    end_stroke();
    const SceneObject* after = scene_.object(before.id);
    if (after == nullptr) throw std::invalid_argument("Scene object was removed during edit");
    if (!same_object(before, *after)) record(ObjectChange{before, *after});
}

void SceneHistory::apply(const Command& command, bool forward) {
    if (const auto* tiles = std::get_if<TileStroke>(&command)) {
        for (const auto& change : *tiles) {
            scene_.map().set(change.tile, forward ? change.after : change.before);
        }
        return;
    }
    const auto& change = std::get<ObjectChange>(command);
    const auto& target = forward ? change.after : change.before;
    const auto& source = forward ? change.before : change.after;
    const std::uint64_t id = target ? target->id : source->id;
    if (!target) {
        (void)scene_.erase(id);
    } else if (SceneObject* object = scene_.object(id)) {
        *object = *target;
    } else {
        scene_.restore(*target);
    }
}

bool SceneHistory::undo() {
    end_stroke();
    if (undo_.empty()) return false;
    Command command = std::move(undo_.back());
    undo_.pop_back();
    apply(command, false);
    redo_.push_back(std::move(command));
    return true;
}

bool SceneHistory::redo() {
    end_stroke();
    if (redo_.empty()) return false;
    Command command = std::move(redo_.back());
    redo_.pop_back();
    apply(command, true);
    undo_.push_back(std::move(command));
    return true;
}

void SceneHistory::clear() noexcept {
    stroke_.clear();
    stroke_lookup_.clear();
    undo_.clear();
    redo_.clear();
}

} // namespace phantasm::iso
