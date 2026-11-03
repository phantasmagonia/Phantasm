#include <phantasm/application.hpp>
#include <phantasm/async_chunk_stream.hpp>
#include <phantasm/chunk_stream.hpp>
#include <phantasm/frame_stats.hpp>
#include <phantasm/log.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

namespace {

constexpr float kChunkSize = 256.0F;
constexpr int kSpritesPerChunk = 8192;
constexpr int kWarmupFrames = 30;
constexpr int kMeasuredFrames = 240;
constexpr double kMsPerSecond = 1000.0;

struct ChunkData {
    std::vector<phantasm::Sprite> sprites;
};

ChunkData generate_chunk(phantasm::ChunkCoord coord) {
    ChunkData chunk;
    chunk.sprites.reserve(kSpritesPerChunk);
    std::uint64_t seed = 0xD6E8FEB86659FD93ULL ^
                         (static_cast<std::uint64_t>(coord.x) * 0x9E3779B97F4A7C15ULL) ^
                         (static_cast<std::uint64_t>(coord.y) * 0xBF58476D1CE4E5B9ULL);
    for (int i = 0; i < kSpritesPerChunk; ++i) {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        const float x = static_cast<float>(seed & 0xFFFFU) / 65536.0F;
        const float y = static_cast<float>((seed >> 16) & 0xFFFFU) / 65536.0F;
        const phantasm::Vec2 position{
            static_cast<float>(coord.x) * kChunkSize + x * (kChunkSize - 8.0F),
            static_cast<float>(coord.y) * kChunkSize + y * (kChunkSize - 8.0F)};
        chunk.sprites.push_back({.position = position,
                                 .size = {8.0F, 8.0F},
                                 .depth = position.y});
    }
    return chunk;
}

class StreamBench final : public phantasm::Application {
public:
    explicit StreamBench(bool background)
        : Application{{.title = "Phantasm Stream Benchmark", .width = 1280, .height = 720}},
          texture_{renderer().create_texture(1, 1, kWhitePixel)},
          synchronous_{kChunkSize, 1}, background_{background} {
        if (background_) {
            asynchronous_.emplace(kChunkSize, 1, generate_chunk);
        }
        if (!renderer().set_vsync(false)) {
            phantasm::log_warn("Vsync could not be disabled; frame timing may be capped");
        }
        renderer().set_camera({.zoom = 4.0F});
    }

private:
    void on_update(double delta_seconds) override {
        if (frame_ > kWarmupFrames && frame_ <= kWarmupFrames + kMeasuredFrames) {
            const auto& stats = renderer().last_stats();
            frame_ms_.push_back(delta_seconds * kMsPerSecond);
            submit_ms_.push_back(stats.submit_ms);
            end_frame_ms_.push_back(stats.cpu_ms);
            visible_.push_back(static_cast<double>(stats.sprites));
            resident_.push_back(static_cast<double>(last_resident_));
        }
        if (frame_ >= kWarmupFrames + kMeasuredFrames) {
            report();
            request_quit();
            return;
        }
        phantasm::Camera camera = renderer().camera();
        camera.position.x = static_cast<float>(frame_) * 32.0F;
        renderer().set_camera(camera);
        ++frame_;
    }

    void on_render() override {
        const auto view = renderer().visible_rect();
        if (background_) {
            auto& stream = *asynchronous_;
            requests_ += stream.update(view, 4);
            completions_ += stream.poll();
            for (const auto& failure : stream.take_failures()) {
                phantasm::log_error("Chunk load failed at ({}, {})", failure.coord.x,
                                    failure.coord.y);
            }
            last_resident_ = stream.size();
            draw_chunks(stream.loaded(), view);
        } else {
            const auto loaded = synchronous_.update(view, 2, generate_chunk);
            requests_ += loaded;
            completions_ += loaded;
            last_resident_ = synchronous_.size();
            draw_chunks(synchronous_.loaded(), view);
        }
    }

    void draw_chunks(const std::map<phantasm::ChunkCoord, ChunkData>& chunks,
                     phantasm::Rect view) {
        for (const auto& [coord, data] : chunks) {
            const phantasm::Rect bounds{static_cast<float>(coord.x) * kChunkSize,
                                        static_cast<float>(coord.y) * kChunkSize,
                                        kChunkSize, kChunkSize};
            if (!phantasm::intersects(bounds, view)) {
                continue;
            }
            for (const auto& sprite : data.sprites) {
                renderer().draw_sprite(texture_, sprite);
            }
        }
    }

    void report() const {
        const auto frame = phantasm::summarize(frame_ms_);
        const auto submit = phantasm::summarize(submit_ms_);
        const auto end = phantasm::summarize(end_frame_ms_);
        const auto visible = phantasm::summarize(visible_);
        const auto resident = phantasm::summarize(resident_);
        phantasm::log_info(
            "mode={} frames={} frame_p50={:.2f}ms frame_p99={:.2f}ms "
            "submit_mean={:.2f}ms submit_p99={:.2f}ms end_frame_mean={:.2f}ms "
            "visible_mean={:.0f} resident_mean={:.1f} requests={} completions={}",
            background_ ? "background" : "synchronous", frame.count, frame.p50, frame.p99,
            submit.mean, submit.p99, end.mean, visible.mean, resident.mean, requests_,
            completions_);
    }

    static constexpr std::array<std::byte, 4> kWhitePixel{
        std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
    phantasm::Texture texture_;
    phantasm::ChunkStream<ChunkData> synchronous_;
    std::optional<phantasm::AsyncChunkStream<ChunkData>> asynchronous_;
    bool background_;
    int frame_ = 0;
    std::size_t requests_ = 0;
    std::size_t completions_ = 0;
    std::size_t last_resident_ = 0;
    std::vector<double> frame_ms_;
    std::vector<double> submit_ms_;
    std::vector<double> end_frame_ms_;
    std::vector<double> visible_;
    std::vector<double> resident_;
};

} // namespace

int main(int argc, char** argv) {
    const bool background = argc == 2 && std::string_view{argv[1]} == "--background";
    if (argc > 2 || (argc == 2 && !background)) {
        phantasm::log_error("Usage: phantasm_stream_bench [--background]");
        return 2;
    }
    try {
        StreamBench app{background};
        app.run();
    } catch (const std::exception& error) {
        phantasm::log_error("Fatal: {}", error.what());
        return 1;
    }
    return 0;
}
