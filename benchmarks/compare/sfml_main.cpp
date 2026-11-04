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
    auto sprites = compare::make_sprites(options.count);
    // One vertex array drawn in a single call: SFML's recommended fast path.
    sf::VertexArray vertices{sf::PrimitiveType::Triangles, options.count * 6};
    compare::FrameTimer timer;

    while (timer.tick()) {
        if (window) {
            while (const auto event = window->pollEvent()) {
                if (event->is<sf::Event::Closed>()) {
                    window->close();
                }
            }
        }
        compare::update(sprites, timer.dt());
        if (options.sorted) {
            compare::sort_by_y(sprites);
        }
        fill_vertices(vertices, sprites);
        target.clear(sf::Color{20, 22, 28});
        target.draw(vertices, &texture);
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
    timer.report("sfml", options);
}
