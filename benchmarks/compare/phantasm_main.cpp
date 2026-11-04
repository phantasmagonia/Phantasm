#include "workload.hpp"

#include <phantasm/application.hpp>
#include <phantasm/sprite.hpp>
#include <phantasm/texture.hpp>

#include <cstddef>
#include <optional>
#include <cstdio>
#include <cstdlib>
#include <span>

namespace {

class Bench final : public phantasm::Application {
public:
    explicit Bench(compare::Options options)
        : Application{{.title = "Phantasm", .width = compare::kWindowWidth,
                       .height = compare::kWindowHeight, .hidden = options.offscreen}},
          options_{options},
          sprites_{options.iso ? std::vector<compare::Sprite>{} : compare::make_sprites(options.count)},
          texture_{make_texture()} {
        if (options.iso) {
            iso_.emplace(options.count);
            const auto pixels = compare::iso_atlas_pixels();
            atlas_.emplace(renderer().create_texture(compare::kAtlasW, compare::kAtlasH,
                                                     std::as_bytes(std::span{pixels})));
        }
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

    // PHANTASM_SCREENSHOT=<file.ppm> saves the last offscreen frame (visual check).
    void save_screenshot() {
        const char* path = std::getenv("PHANTASM_SCREENSHOT");
        if (path == nullptr || !options_.offscreen) {
            return;
        }
        const auto rgba = renderer().read_offscreen_pixels();
        std::FILE* file = std::fopen(path, "wb");
        if (file == nullptr) {
            return;
        }
        std::fprintf(file, "P6 %d %d 255\n", compare::kWindowWidth, compare::kWindowHeight);
        for (std::size_t i = 0; i < rgba.size(); i += 4) {
            std::fwrite(&rgba[i], 1, 3, file);
        }
        std::fclose(file);
    }

protected:
    void on_update(double) override {
        if (!timer_.tick()) {
            request_quit();
        }
        const auto& stats = renderer().last_stats();
        submit_ms_ += stats.submit_ms;
        end_frame_ms_ += stats.cpu_ms;
        ++stat_frames_;
        if (iso_) {
            iso_->update(timer_.dt());
        } else {
            compare::update(sprites_, timer_.dt());
        }
    }

    void on_render() override {
        if (iso_) {
            render_iso();
            return;
        }
        constexpr float kSize = compare::kSpriteSize;
        for (const compare::Sprite& s : sprites_) {
            // Phantasm sorts by depth itself; "plain" uses one depth for all sprites.
            renderer().draw_sprite(texture_, {.position = {s.x, s.y},
                                              .size = {kSize, kSize},
                                              .depth = options_.sorted ? s.y : 0.0F});
        }
    }

private:
    // Floors on layer 0 in submission order; walls and characters on layer 1, sorted by
    // the engine using their depth.
    void render_iso() {
        for (const compare::IsoSprite& floor : iso_->floors()) {
            draw_iso(floor, 0);
        }
        for (const compare::IsoSprite& object : iso_->objects(false)) {
            draw_iso(object, 1);
        }
    }

    void draw_iso(const compare::IsoSprite& s, int layer) {
        constexpr float kU = 1.0F / compare::kAtlasW;
        constexpr float kV = 1.0F / compare::kAtlasH;
        renderer().draw_sprite(*atlas_, {.position = {s.x, s.y},
                                         .size = {s.region.w, s.region.h},
                                         .uv = {s.region.u * kU, s.region.v * kV,
                                                s.region.w * kU, s.region.h * kV},
                                         .layer = layer,
                                         .depth = s.depth});
    }

    phantasm::Texture make_texture() {
        const auto pixels = compare::blob_pixels();
        return renderer().create_texture(compare::kSpriteSize, compare::kSpriteSize,
                                         std::as_bytes(std::span{pixels}));
    }

    compare::Options options_;
    std::vector<compare::Sprite> sprites_;
    phantasm::Texture texture_;
    std::optional<compare::IsoScene> iso_;
    std::optional<phantasm::Texture> atlas_;
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
    bench.save_screenshot();
}
