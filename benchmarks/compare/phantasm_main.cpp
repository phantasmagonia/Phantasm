#include "workload.hpp"

#include <phantasm/application.hpp>
#include <phantasm/sprite.hpp>
#include <phantasm/texture.hpp>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <span>

namespace {

class Bench final : public phantasm::Application {
public:
    explicit Bench(compare::Options options)
        : Application{{.title = "Phantasm", .width = compare::kWindowWidth,
                       .height = compare::kWindowHeight, .hidden = options.offscreen}},
          options_{options}, sprites_{compare::make_sprites(options.count)},
          texture_{make_texture()} {
        renderer().set_vsync(false);
        if (options.offscreen) {
            renderer().set_offscreen(compare::kWindowWidth, compare::kWindowHeight);
        }
        // Camera centered on the window so world units equal screen pixels.
        renderer().set_camera({.position = {compare::kWindowWidth / 2.0F,
                                            compare::kWindowHeight / 2.0F}});
    }

    compare::FrameTimer& timer() { return timer_; }
    // PHANTASM_STATS=1 prints where CPU time goes (stderr, so run.sh output stays CSV).
    void print_breakdown() const {
        if (std::getenv("PHANTASM_STATS") != nullptr && stat_frames_ > 0) {
            std::fprintf(stderr, "submit (draw_sprite calls + end_frame) %.3f ms, end_frame %.3f ms\n",
                         submit_ms_ / stat_frames_, end_frame_ms_ / stat_frames_);
        }
    }
    void wait_for_gpu() { renderer().wait_idle(); }

protected:
    void on_update(double) override {
        if (!timer_.tick()) {
            request_quit();
        }
        const auto& stats = renderer().last_stats();
        submit_ms_ += stats.submit_ms;
        end_frame_ms_ += stats.cpu_ms;
        ++stat_frames_;
        compare::update(sprites_, timer_.dt());
    }

    void on_render() override {
        constexpr float kSize = compare::kSpriteSize;
        for (const compare::Sprite& s : sprites_) {
            // Phantasm sorts by depth itself; "plain" uses one depth for all sprites.
            renderer().draw_sprite(texture_, {.position = {s.x, s.y},
                                              .size = {kSize, kSize},
                                              .depth = options_.sorted ? s.y : 0.0F});
        }
    }

private:
    phantasm::Texture make_texture() {
        const auto pixels = compare::blob_pixels();
        return renderer().create_texture(compare::kSpriteSize, compare::kSpriteSize,
                                         std::as_bytes(std::span{pixels}));
    }

    compare::Options options_;
    std::vector<compare::Sprite> sprites_;
    phantasm::Texture texture_;
    compare::FrameTimer timer_;
    double submit_ms_ = 0.0;
    double end_frame_ms_ = 0.0;
    int stat_frames_ = 0;
};

} // namespace

int main(int argc, char** argv) {
    const auto options = compare::parse(argc, argv);
    Bench bench{options};
    bench.run();
    bench.wait_for_gpu();
    bench.timer().finish();
    bench.timer().report("phantasm", options);
    bench.print_breakdown();
}
