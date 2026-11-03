#include <phantasm/application.hpp>
#include <phantasm/log.hpp>
#include <phantasm/paths.hpp>
#include <phantasm/texture.hpp>

#include <array>
#include <cstddef>
#include <exception>

namespace {

constexpr float kSpriteScale = 8.0F;
constexpr float kMoveSpeed = 400.0F; // pixels per second

class Sandbox final : public phantasm::Application {
public:
    Sandbox()
        : Application{{.title = "Phantasm Sandbox", .width = 1280, .height = 720}},
          slime_{renderer().load_texture(phantasm::asset_path("assets/slime.png"))} {}

protected:
    void on_update(double delta_seconds) override {
        using phantasm::Key;

        if (input().was_pressed(Key::Escape)) {
            request_quit();
        }
        if (input().was_pressed(Key::Space)) {
            next_color();
        }

        const auto step = kMoveSpeed * static_cast<float>(delta_seconds);
        if (input().is_down(Key::Left) || input().is_down(Key::A)) {
            position_.x -= step;
        }
        if (input().is_down(Key::Right) || input().is_down(Key::D)) {
            position_.x += step;
        }
        if (input().is_down(Key::Up) || input().is_down(Key::W)) {
            position_.y -= step;
        }
        if (input().is_down(Key::Down) || input().is_down(Key::S)) {
            position_.y += step;
        }

        elapsed_ += delta_seconds;
        ++frames_;
        if (elapsed_ >= 1.0) {
            phantasm::log_info("{} fps", frames_);
            elapsed_ = 0.0;
            frames_ = 0;
        }
    }

    void on_render() override {
        renderer().draw_sprite(slime_, {.x = position_.x,
                                        .y = position_.y,
                                        .w = static_cast<float>(slime_.width()) * kSpriteScale,
                                        .h = static_cast<float>(slime_.height()) * kSpriteScale});
    }

private:
    void next_color() {
        static constexpr std::array kColors{
            phantasm::Color{0.10F, 0.10F, 0.12F, 1.0F},
            phantasm::Color{0.39F, 0.58F, 0.93F, 1.0F},
            phantasm::Color{0.85F, 0.35F, 0.30F, 1.0F},
            phantasm::Color{0.95F, 0.85F, 0.60F, 1.0F},
        };
        color_index_ = (color_index_ + 1) % kColors.size();
        set_clear_color(kColors[color_index_]);
    }

    phantasm::Texture slime_;
    phantasm::Vec2 position_{576.0F, 296.0F};
    std::size_t color_index_ = 0;
    double elapsed_ = 0.0;
    int frames_ = 0;
};

} // namespace

int main() {
    try {
        Sandbox app;
        app.run();
    } catch (const std::exception& error) {
        phantasm::log_error("Fatal: {}", error.what());
        return 1;
    }
    return 0;
}
