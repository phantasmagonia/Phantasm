#include <phantasm/application.hpp>

#include <phantasm/frame_clock.hpp>
#include <phantasm/log.hpp>

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>

namespace phantasm {

namespace detail {

SdlContext::SdlContext() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string{"SDL_Init failed: "} + SDL_GetError());
    }
    log_info("SDL {}.{}.{} initialized", SDL_MAJOR_VERSION, SDL_MINOR_VERSION,
             SDL_MICRO_VERSION);
}

SdlContext::~SdlContext() {
    SDL_Quit();
}

} // namespace detail

namespace {

constexpr Uint32 kIdleSleepMs = 16;

bool to_key(SDL_Scancode scancode, Key& out) noexcept {
    const auto index = static_cast<std::size_t>(scancode);
    if (index >= kKeyCount) {
        return false;
    }
    out = static_cast<Key>(index);
    return true;
}

} // namespace

Application::Application(const WindowConfig& config) : window_{config}, renderer_{window_} {}

void Application::run() {
    running_ = true;
    FrameClock clock{SDL_GetTicksNS()};

    while (running_) {
        input_.begin_frame();
        pump_events();
        on_update(clock.tick(SDL_GetTicksNS()));

        const bool drawing = renderer_.begin_frame(clear_color_);
        if (drawing) {
            on_render();
        }
        renderer_.end_frame();
        // No vsync wait when nothing is presented; avoid spinning the CPU.
        if (!drawing) {
            SDL_Delay(kIdleSleepMs);
        }
    }
}

void Application::request_quit() noexcept {
    running_ = false;
}

void Application::pump_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            request_quit();
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            Key key{};
            if (to_key(event.key.scancode, key)) {
                input_.set_key(key, event.key.down);
            }
            break;
        }
        default:
            break;
        }
    }
}

} // namespace phantasm
