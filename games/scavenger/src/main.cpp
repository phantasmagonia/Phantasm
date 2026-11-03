#include "tiles.hpp"

#include <phantasm/application.hpp>
#include <phantasm/iso/iso_grid.hpp>
#include <phantasm/iso/tile_map.hpp>
#include <phantasm/log.hpp>
#include <phantasm/paths.hpp>
#include <phantasm/sprite.hpp>
#include <phantasm/texture.hpp>

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using phantasm::Vec2;
using phantasm::iso::GridPos;
using phantasm::iso::TileCoord;
using phantasm::iso::TileMap;

constexpr float kArtScale = 2.0F;
constexpr float kCameraSpeed = 500.0F;
constexpr float kZoomStep = 1.5F;
constexpr float kMinZoom = 0.5F;
constexpr float kMaxZoom = 4.0F;

// Pivots: where each image touches its tile. Floors are centered on the tile;
// the wall block's base diamond center sits 16 art px above its bottom edge.
constexpr Vec2 kFloorPivot{0.5F, 0.5F};
constexpr Vec2 kWallPivot{0.5F, 56.0F / 64.0F};
constexpr Vec2 kStandingPivot{0.5F, 0.95F};

std::string asset(const std::string& name) {
    return phantasm::asset_path("assets/scavenger/" + name);
}

Vec2 scaled_size(const phantasm::Texture& texture) {
    return Vec2{static_cast<float>(texture.width()) * kArtScale,
                static_cast<float>(texture.height()) * kArtScale};
}

GridPos tile_center(TileCoord tile) {
    return GridPos{static_cast<float>(tile.x) + 0.5F, static_cast<float>(tile.y) + 0.5F,
                   tile.level};
}

// Floors of a level draw first; walls, items, and characters then sort by depth.
int floor_layer(int level) {
    return level * 2;
}
int object_layer(int level) {
    return level * 2 + 1;
}

class Scavenger final : public phantasm::Application {
public:
    Scavenger()
        : Application{{.title = "Scavenger", .width = 1280, .height = 720}},
          map_{TileMap::parse(phantasm::read_text_file(asset("house.map")))},
          floor_wood_{renderer().load_texture(asset("floor_wood.png"))},
          floor_grass_{renderer().load_texture(asset("floor_grass.png"))},
          wall_{renderer().load_texture(asset("wall.png"))},
          player_{renderer().load_texture(asset("player.png"))},
          supply_{renderer().load_texture(asset("supply.png"))} {
        const auto starts = map_.find(scavenger::tiles::kPlayerStart);
        if (starts.size() != 1) {
            throw std::runtime_error("Map needs exactly one player start 'P'");
        }
        player_position_ = tile_center(starts.front());
        renderer().set_camera({.position = grid_.grid_to_world(player_position_), .zoom = 1.0F});
        set_clear_color({0.08F, 0.09F, 0.12F, 1.0F});
        phantasm::log_info("Map {}x{}, {} level(s)", map_.width(), map_.height(),
                           map_.level_count());
    }

protected:
    void on_update(double delta_seconds) override {
        using phantasm::Key;
        const auto dt = static_cast<float>(delta_seconds);
        if (input().was_pressed(Key::Escape)) {
            request_quit();
        }

        phantasm::Camera camera = renderer().camera();
        const float step = kCameraSpeed * dt / camera.zoom;
        if (input().is_down(Key::A) || input().is_down(Key::Left)) {
            camera.position.x -= step;
        }
        if (input().is_down(Key::D) || input().is_down(Key::Right)) {
            camera.position.x += step;
        }
        if (input().is_down(Key::W) || input().is_down(Key::Up)) {
            camera.position.y -= step;
        }
        if (input().is_down(Key::S) || input().is_down(Key::Down)) {
            camera.position.y += step;
        }
        if (input().was_pressed(Key::E)) {
            camera.zoom = std::min(camera.zoom * kZoomStep, kMaxZoom);
        }
        if (input().was_pressed(Key::Q)) {
            camera.zoom = std::max(camera.zoom / kZoomStep, kMinZoom);
        }
        renderer().set_camera(camera);
    }

    void on_render() override {
        for (int level = 0; level < map_.level_count(); ++level) {
            for (int y = 0; y < map_.height(); ++y) {
                for (int x = 0; x < map_.width(); ++x) {
                    draw_tile(TileCoord{x, y, level});
                }
            }
        }
        draw_standing(player_, player_position_);
    }

private:
    void draw_tile(TileCoord tile) {
        namespace tiles = scavenger::tiles;
        const char kind = map_.at(tile);
        const GridPos center = tile_center(tile);

        if (kind == tiles::kGrass || kind == tiles::kPlayerStart) {
            draw_floor(floor_grass_, center);
        } else if (tiles::is_indoor_floor(kind)) {
            draw_floor(floor_wood_, center);
        }

        if (kind == tiles::kWall) {
            draw_object(wall_, center, kWallPivot);
        } else if (kind == tiles::kSupply) {
            draw_standing(supply_, center);
        }
    }

    void draw_floor(const phantasm::Texture& texture, GridPos at) {
        renderer().draw_sprite(texture, {.position = grid_.grid_to_world(at),
                                         .size = scaled_size(texture),
                                         .pivot = kFloorPivot,
                                         .layer = floor_layer(at.level),
                                         .depth = phantasm::iso::draw_depth(at)});
    }

    void draw_object(const phantasm::Texture& texture, GridPos at, Vec2 pivot) {
        renderer().draw_sprite(texture, {.position = grid_.grid_to_world(at),
                                         .size = scaled_size(texture),
                                         .pivot = pivot,
                                         .layer = object_layer(at.level),
                                         .depth = phantasm::iso::draw_depth(at)});
    }

    void draw_standing(const phantasm::Texture& texture, GridPos at) {
        draw_object(texture, at, kStandingPivot);
    }

    phantasm::iso::IsoGrid grid_{};
    TileMap map_;
    phantasm::Texture floor_wood_;
    phantasm::Texture floor_grass_;
    phantasm::Texture wall_;
    phantasm::Texture player_;
    phantasm::Texture supply_;
    GridPos player_position_{};
};

} // namespace

int main() {
    try {
        Scavenger game;
        game.run();
    } catch (const std::exception& error) {
        phantasm::log_error("Fatal: {}", error.what());
        return 1;
    }
    return 0;
}
