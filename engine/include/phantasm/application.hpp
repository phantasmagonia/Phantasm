#pragma once

#include <phantasm/input.hpp>
#include <phantasm/window.hpp>

namespace phantasm {

namespace detail {
// Initializes SDL on construction and shuts it down on destruction (RAII).
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

// Base class for a game. Derive from it, override on_update, call run().
class Application {
public:
    explicit Application(const WindowConfig& config = {});
    virtual ~Application() = default;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    // Runs the main loop until the window closes or request_quit() is called.
    void run();
    void request_quit() noexcept;

protected:
    // Called once per frame with the seconds elapsed since the last frame.
    virtual void on_update(double delta_seconds) = 0;

    [[nodiscard]] const Input& input() const noexcept { return input_; }

private:
    void pump_events();

    // ORDER MATTERS: members are constructed top-to-bottom and destroyed
    // bottom-to-top. SDL must be initialized before the window exists and
    // shut down only after the window is gone.
    detail::SdlContext sdl_;
    Window window_;
    Input input_;
    bool running_ = false;
};

} // namespace phantasm
