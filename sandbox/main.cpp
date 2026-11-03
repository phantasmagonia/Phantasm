#include <phantasm/application.hpp>
#include <phantasm/log.hpp>

#include <array>
#include <cstddef>
#include <exception>

namespace {

class Sandbox final : public phantasm::Application {
public:
    Sandbox() : Application{{.title = "Phantasm Sandbox", .width = 1280, .height = 720}} {}

protected:
    void on_update(double delta_seconds) override {
        using phantasm::Key;

        if (input().was_pressed(Key::Escape)) {
            request_quit();
        }
        if (input().was_pressed(Key::Space)) {
            phantasm::log_info("Space pressed");
            next_color();
        }
        if (input().was_released(Key::Space)) {
            phantasm::log_info("Space released");
        }

        elapsed_ += delta_seconds;
        ++frames_;
        if (elapsed_ >= 1.0) {
            phantasm::log_info("{} fps", frames_);
            elapsed_ = 0.0;
            frames_ = 0;
        }
    }

private:
    void next_color() {
        static constexpr std::array kColors{
            phantasm::Color{0.10F, 0.10F, 0.12F, 1.0F},
            phantasm::Color{0.39F, 0.58F, 0.93F, 1.0F},
            phantasm::Color{0.85F, 0.35F, 0.30F, 1.0F},
            phantasm::Color{0.30F, 0.70F, 0.40F, 1.0F},
        };
        color_index_ = (color_index_ + 1) % kColors.size();
        set_clear_color(kColors[color_index_]);
    }

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
