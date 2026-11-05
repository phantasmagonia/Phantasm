#include <phantasm/application.hpp>

#include <phantasm/frame_clock.hpp>
#include <phantasm/log.hpp>

#include <SDL3/SDL.h>

#include <optional>
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

namespace {

std::optional<MouseButton> to_mouse_button(Uint8 button) noexcept {
    switch (button) {
    case SDL_BUTTON_LEFT: return MouseButton::Left;
    case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
    case SDL_BUTTON_RIGHT: return MouseButton::Right;
    case SDL_BUTTON_X1: return MouseButton::X1;
    case SDL_BUTTON_X2: return MouseButton::X2;
    default: return std::nullopt;
    }
}

} // namespace

Application::Application(const WindowConfig& config) : window_{config}, renderer_{window_} {}

void Application::run() {
    running_ = true;
    FrameClock clock{SDL_GetTicksNS()};

    while (running_) {
        input_.begin_frame();
        pump_events();
        if (!running_) {
            break;
        }
        on_update(clock.tick(SDL_GetTicksNS()));
        if (!running_) {
            break;
        }

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
        case SDL_EVENT_MOUSE_MOTION:
            input_.set_mouse_position({event.motion.x, event.motion.y});
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (const auto button = to_mouse_button(event.button.button)) {
                input_.set_mouse_button(*button, event.button.down);
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            input_.add_mouse_wheel(event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                                       ? -event.wheel.y
                                       : event.wheel.y);
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

Vec2 Application::pointer_world() const noexcept {
    const float density = window_.pixel_density();
    const Vec2 mouse = input_.mouse_position();
    return renderer_.screen_to_world({mouse.x * density, mouse.y * density});
}

} // namespace phantasm
