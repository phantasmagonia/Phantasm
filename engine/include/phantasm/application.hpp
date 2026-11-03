#pragma once

#include <phantasm/color.hpp>
#include <phantasm/input.hpp>
#include <phantasm/renderer.hpp>
#include <phantasm/window.hpp>

namespace phantasm {

namespace detail {
class SdlContext {
public:
    SdlContext();
    ~SdlContext();
    SdlContext(const SdlContext&) = delete;
    SdlContext& operator=(const SdlContext&) = delete;
    SdlContext(SdlContext&&) = delete;
    SdlContext& operator=(SdlContext&&) = delete;
};
} // namespace detail

class Application {
public:
    explicit Application(const WindowConfig& config = {});
    virtual ~Application() = default;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void run();
    void request_quit() noexcept;

protected:
    virtual void on_update(double delta_seconds) = 0;

    [[nodiscard]] const Input& input() const noexcept { return input_; }

    void set_clear_color(Color color) noexcept { clear_color_ = color; }

private:
    void pump_events();

    // Declaration order is construction order (destruction is reverse):
    // SDL must outlive the window, and the window must outlive the renderer.
    detail::SdlContext sdl_;
    Window window_;
    Renderer renderer_;
    Input input_;
    Color clear_color_{0.1F, 0.1F, 0.12F, 1.0F};
    bool running_ = false;
};

} // namespace phantasm
