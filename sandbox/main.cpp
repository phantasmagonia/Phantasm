// Sandbox: a tiny app for trying engine features by hand.
#include <phantasm/application.hpp>
#include <phantasm/log.hpp>

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
        }
        if (input().was_released(Key::Space)) {
            phantasm::log_info("Space released");
        }

        // Report frames per second once per second.
        elapsed_ += delta_seconds;
        ++frames_;
        if (elapsed_ >= 1.0) {
            phantasm::log_info("{} fps", frames_);
            elapsed_ = 0.0;
            frames_ = 0;
        }
    }

private:
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
