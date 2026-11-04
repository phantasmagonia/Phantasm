#include <phantasm/iso/scene.hpp>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace phantasm::iso {
namespace {

std::string_view take_line(std::string_view& input) {
    const auto end = input.find('\n');
    if (end == std::string_view::npos) {
        throw std::runtime_error("Scene has a truncated line");
    }
    const auto line = input.substr(0, end);
    input.remove_prefix(end + 1);
    return line;
}

template <typename T>
T read_header(std::string_view line, std::string_view prefix) {
    if (!line.starts_with(prefix)) {
        throw std::runtime_error("Scene expected " + std::string{prefix});
    }
    std::istringstream stream{std::string{line.substr(prefix.size())}};
    stream.imbue(std::locale::classic());
    T value{};
    stream >> value;
    if (!stream || !(stream >> std::ws).eof()) {
        throw std::runtime_error("Scene has an invalid " + std::string{prefix} + " value");
    }
    return value;
}

void check_position(GridPos position, int levels) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || position.level < 0 ||
        position.level >= levels) {
        throw std::invalid_argument("Scene object has an invalid position or level");
    }
}

} // namespace

Scene::Scene(TileMap map) : map_{std::move(map)} {
    if (map_.level_count() == 0) {
        throw std::invalid_argument("Scene requires a nonempty tile map");
    }
}

void Scene::insert(std::uint64_t id, GridPos position, std::string name) {
    check_position(position, map_.level_count());
    if (id == 0 || name.find_first_of("\r\n") != std::string::npos || ids_.contains(id)) {
        throw std::invalid_argument("Scene object has an invalid or duplicate ID or name");
    }
    const Entity entity = store_->create(SceneObject{id, position, std::move(name)});
    try {
        ids_.emplace(id, entity);
    } catch (...) {
        store_->destroy(entity);
        throw;
    }
}

std::uint64_t Scene::create(GridPos position, std::string name) {
    if (next_id_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("Scene ID space exhausted");
    }
    const auto id = next_id_;
    insert(id, position, std::move(name));
    ++next_id_;
    return id;
}

bool Scene::erase(std::uint64_t id) {
    const auto it = ids_.find(id);
    if (it == ids_.end()) {
        return false;
    }
    store_->destroy(it->second);
    ids_.erase(it);
    return true;
}

SceneObject* Scene::object(std::uint64_t id) noexcept {
    const auto it = ids_.find(id);
    return it == ids_.end() ? nullptr : store_->get<SceneObject>(it->second);
}

const SceneObject* Scene::object(std::uint64_t id) const noexcept {
    return const_cast<Scene*>(this)->object(id);
}

Entity Scene::runtime_entity(std::uint64_t id) const noexcept {
    const auto it = ids_.find(id);
    return it == ids_.end() ? Entity{} : it->second;
}

std::string Scene::serialize() const {
    const std::string map_text = map_.serialize();
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "phantasm-scene 1\nmap-bytes " << map_text.size() << '\n' << map_text;
    out << "entities " << ids_.size() << " next-id " << next_id_ << '\n';
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    for (const auto& [id, entity] : ids_) {
        const SceneObject* value = store_->get<SceneObject>(entity);
        if (value == nullptr || value->id != id) {
            throw std::logic_error("Scene object ID index is inconsistent");
        }
        check_position(value->position, map_.level_count());
        if (value->name.find_first_of("\r\n") != std::string::npos) {
            throw std::invalid_argument("Scene object name contains a line break");
        }
        out << id << ' ' << value->position.x << ' ' << value->position.y << ' '
            << value->position.level << ' ' << std::quoted(value->name) << '\n';
    }
    return out.str();
}

Scene Scene::parse(std::string_view text) {
    if (take_line(text) != "phantasm-scene 1") {
        throw std::runtime_error("Unsupported scene format version");
    }
    const std::size_t map_bytes = read_header<std::size_t>(take_line(text), "map-bytes ");
    if (map_bytes > text.size()) {
        throw std::runtime_error("Scene map data is truncated");
    }
    Scene scene{TileMap::parse(text.substr(0, map_bytes))};
    text.remove_prefix(map_bytes);
    const auto header = take_line(text);
    std::istringstream counts{std::string{header}};
    counts.imbue(std::locale::classic());
    std::string label;
    std::string next_label;
    std::size_t count = 0;
    counts >> label >> count >> next_label >> scene.next_id_;
    if (!counts || label != "entities" || next_label != "next-id" || scene.next_id_ == 0 ||
        !(counts >> std::ws).eof()) {
        throw std::runtime_error("Scene entity header is invalid");
    }
    for (std::size_t i = 0; i < count; ++i) {
        std::istringstream row{std::string{take_line(text)}};
        row.imbue(std::locale::classic());
        std::uint64_t id = 0;
        GridPos position;
        std::string name;
        row >> id >> position.x >> position.y >> position.level >> std::quoted(name);
        if (!row || !(row >> std::ws).eof() || id >= scene.next_id_) {
            throw std::runtime_error("Scene entity row is invalid");
        }
        try {
            scene.insert(id, position, std::move(name));
        } catch (const std::invalid_argument& error) {
            throw std::runtime_error(error.what());
        }
    }
    if (!text.empty()) {
        throw std::runtime_error("Scene has trailing data");
    }
    return scene;
}

Scene Scene::load_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("Could not open scene file for reading: " + path.string());
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.eof() && input.fail()) {
        throw std::runtime_error("Could not read scene file: " + path.string());
    }
    return parse(contents.str());
}

void Scene::save_file(const std::filesystem::path& path) const {
    const std::string contents = serialize();
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output) {
        throw std::runtime_error("Could not open scene file for writing: " + path.string());
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    if (!output) {
        throw std::runtime_error("Could not write scene file: " + path.string());
    }
}

} // namespace phantasm::iso
