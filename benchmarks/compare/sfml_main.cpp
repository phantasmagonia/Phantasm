#include "workload.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#include <optional>

namespace {

void fill_vertices(sf::VertexArray& vertices, const std::vector<compare::Sprite>& sprites) {
    constexpr float kSize = compare::kSpriteSize;
    for (std::size_t i = 0; i < sprites.size(); ++i) {
        const float x = sprites[i].x;
        const float y = sprites[i].y;
        const sf::Vector2f corners[6] = {{x, y}, {x + kSize, y}, {x, y + kSize},
                                         {x + kSize, y}, {x + kSize, y + kSize}, {x, y + kSize}};
        const sf::Vector2f uvs[6] = {{0, 0}, {kSize, 0}, {0, kSize},
                                     {kSize, 0}, {kSize, kSize}, {0, kSize}};
        for (std::size_t v = 0; v < 6; ++v) {
            vertices[i * 6 + v].position = corners[v];
            vertices[i * 6 + v].texCoords = uvs[v];
        }
    }
}

void put_quad(sf::VertexArray& vertices, std::size_t quad, const compare::IsoSprite& s) {
    const float x = s.x;
    const float y = s.y;
    const float w = s.region.w;
    const float h = s.region.h;
    const float u = s.region.u;
    const float v = s.region.v;
    const sf::Vector2f corners[6] = {{x, y}, {x + w, y}, {x, y + h},
                                     {x + w, y}, {x + w, y + h}, {x, y + h}};
    const sf::Vector2f uvs[6] = {{u, v}, {u + w, v}, {u, v + h},
                                 {u + w, v}, {u + w, v + h}, {u, v + h}};
    for (std::size_t i = 0; i < 6; ++i) {
        vertices[quad * 6 + i].position = corners[i];
        vertices[quad * 6 + i].texCoords = uvs[i];
    }
}

} // namespace

int main(int argc, char** argv) {
    const auto options = compare::parse(argc, argv);
    constexpr sf::Vector2u kSize{compare::kWindowWidth, compare::kWindowHeight};
    // Offscreen mode needs no window: a RenderTexture has its own context.
    std::optional<sf::RenderWindow> window;
    std::optional<sf::RenderTexture> offscreen;
    if (options.offscreen) {
        offscreen.emplace(kSize);
    } else {
        window.emplace(sf::VideoMode{kSize}, "SFML");
        window->setVerticalSyncEnabled(false);
    }
    sf::RenderTarget& target = options.offscreen ? static_cast<sf::RenderTarget&>(*offscreen)
                                                 : static_cast<sf::RenderTarget&>(*window);

    const auto pixels = compare::blob_pixels();
    const sf::Image image{{compare::kSpriteSize, compare::kSpriteSize}, pixels.data()};
    const sf::Texture texture{image};
    auto sprites = options.iso ? std::vector<compare::Sprite>{} : compare::make_sprites(options.count);
    // One vertex array drawn in a single call: SFML's recommended fast path.
    sf::VertexArray vertices{sf::PrimitiveType::Triangles, options.iso ? 0 : options.count * 6};
    std::optional<compare::IsoScene> iso;
    std::optional<sf::Texture> atlas;
    if (options.iso) {
        iso.emplace(options.count);
        const auto atlas_pixels = compare::iso_atlas_pixels();
        atlas.emplace(sf::Image{{compare::kAtlasW, compare::kAtlasH}, atlas_pixels.data()});
    }
    std::optional<sf::RenderTexture> floor_cache;
    if (options.cached) {
        floor_cache.emplace(kSize);
        floor_cache->clear(sf::Color::Transparent);
        vertices.resize(iso->floors().size() * 6);
        std::size_t quad = 0;
        for (const auto& floor : iso->floors()) {
            put_quad(vertices, quad++, floor);
        }
        floor_cache->draw(vertices, &*atlas);
        floor_cache->display();
    }
    compare::FrameTimer timer;

    while (timer.tick()) {
        if (window) {
            while (const auto event = window->pollEvent()) {
                if (event->is<sf::Event::Closed>()) {
                    window->close();
                }
            }
        }
        target.clear(sf::Color{20, 22, 28});
        if (iso) {
            iso->update(timer.dt());
            const auto& floors = iso->floors();
            const auto& objects = iso->objects(true);
            vertices.resize(((floor_cache ? 0 : floors.size()) + objects.size()) * 6);
            std::size_t quad = 0;
            if (floor_cache) {
                target.draw(sf::Sprite{floor_cache->getTexture()});
            } else {
                for (const auto& floor : floors) {
                    put_quad(vertices, quad++, floor);
                }
            }
            for (const auto& object : objects) {
                put_quad(vertices, quad++, object);
            }
            target.draw(vertices, &*atlas);
        } else {
            compare::update(sprites, timer.dt());
            if (options.sorted) {
                compare::sort_by_y(sprites);
            }
            fill_vertices(vertices, sprites);
            target.draw(vertices, &texture);
        }
        if (offscreen) {
            offscreen->display();
        } else {
            window->display();
        }
    }
    if (offscreen) {
        (void)offscreen->setActive(true);
    } else {
        (void)window->setActive(true);
    }
    glFinish(); // include all queued GPU work in the measurement
    timer.finish();
    if (offscreen && std::getenv("COMPARE_SCREENSHOT") != nullptr) {
        const auto image = offscreen->getTexture().copyToImage();
        compare::save_screenshot({image.getPixelsPtr(), compare::kWindowWidth *
            compare::kWindowHeight * 4}, compare::kWindowWidth, compare::kWindowHeight);
    }
    timer.report("sfml", options);
}
