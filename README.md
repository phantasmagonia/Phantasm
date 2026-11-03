# Phantasm

**Phantasm is an open-source 2D game engine written in modern C++20, built on SDL3 and SDL_GPU.**
It has a genre-neutral core and an optional isometric layer for large, multi-level
isometric worlds. It runs natively on Metal (macOS) and Vulkan (Windows/Linux).

**Status:** early development. Not yet ready for production use.

## Features

Implemented and tested:

- **Platform layer:** window, event loop, keyboard input (down / pressed / released), frame timing
  with clamped delta time, logging via `std::format`.
- **Renderer (SDL_GPU):** textured sprites, PNG/JPEG loading, alpha blending, pixel-art friendly
  nearest filtering.
- **Batching:** sprites are sorted by layer, depth, and texture and drawn with instanced rendering
  in one draw call per texture run. See [Performance](#performance).
- **Camera:** world-space 2D camera with pan and zoom.
- **Sprites:** pivot/anchor point, UV rect, tint, layer, depth.
- **Texture atlas:** packs many images into one texture (stb_rect_pack) so sprites batch together.
- **Isometric layer (`phantasm_iso`):** 2:1 diamond grid ↔ world projection, multiple floor levels,
  tile picking, draw-depth helpers.

Planned: isometric tilemaps, wall cutaway, collision, line of sight, lighting, animation, audio,
chunk streaming, an editor (Dear ImGui), and Direct3D 12 support. See [Roadmap](#roadmap).

## Performance

Performance is a first-class goal. Every number here comes from the committed benchmarks
and states its hardware. Baseline on a **MacBook Air (M1, 8 GB RAM)**, release build:

**End-to-end** (`sandbox --bench`, vsync off, 240 measured frames):

| Sprites | Textures | FPS | Frame p50 | Render CPU | Draw calls |
|---:|---|---:|---:|---:|---:|
| 1,000 | 1 | 351 | 2.6 ms | 0.12 ms | 1 |
| 10,000 | 1 | 283 | 1.6 ms | 0.84 ms | 1 |
| 100,000 | 1 | 88 | 10.4 ms | 9.5 ms | 1 |
| 1,000,000 | 1 | 9 | 117 ms | 107 ms | 1 |
| 100,000 | 16 separate | 34 | 29.4 ms | 28.5 ms | 93,664 |
| 100,000 | 16 in one atlas | 96 | 10.2 ms | 9.3 ms | 1 |

**Texture atlases** (`build_atlas`) keep depth-sorted scenes in one draw call no matter how
many images they use: 2.8x faster than separate textures at 100k sprites.

**Known bottleneck:** sorting dominates render CPU (100k random-depth sprites: ~10 ms;
~2 ms when mostly sorted). Next: radix sort and culling.

Run them yourself:

```sh
cmake --preset release && cmake --build --preset release
./build/release/bin/phantasm_benchmarks     # CPU microbenchmarks (Google Benchmark)
./build/release/bin/sandbox --bench         # GPU end-to-end, vsync off
```

Close other apps first; background load skews results.

## Build

Requirements: CMake 3.26+, Ninja, a C++20 compiler (Clang 17+, MSVC 2022), `glslc`, and `spirv-cross`.
SDL3, stb_image, and Catch2 are downloaded automatically by CMake.

```sh
# macOS
brew install cmake ninja shaderc spirv-cross

cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/bin/sandbox          # optional: sprite count, e.g. 10000
```

On Windows, install the [Vulkan SDK](https://vulkan.lunarg.com/) for `glslc` and `spirv-cross`.

Sandbox controls: WASD/arrows pan, Q/E zoom, 1/2/3 spawn 1k/10k/100k sprites,
Space cycles the background, Esc quits.

## Minimal example

```cpp
#include <phantasm/application.hpp>
#include <phantasm/paths.hpp>

class Game final : public phantasm::Application {
public:
    Game() : hero_{renderer().load_texture(phantasm::asset_path("assets/hero.png"))} {}

protected:
    void on_update(double dt) override {
        if (input().is_down(phantasm::Key::Right)) {
            x_ += 200.0F * static_cast<float>(dt);
        }
    }
    void on_render() override {
        renderer().draw_sprite(hero_, {.position = {x_, 0.0F}, .size = {64.0F, 64.0F}});
    }

private:
    phantasm::Texture hero_;
    float x_ = 0.0F;
};

int main() {
    Game game;
    game.run();
}
```

## Demos

Example games live in [phantasm-demo-projects](https://github.com/hacengeloglu-blip/phantasm-demo-projects),
which builds against a pinned Phantasm commit. This repository contains no art.

## Repository layout

| Path | Contents |
|---|---|
| `engine/` | `phantasm`: genre-neutral core (platform, input, renderer, camera, sprites) |
| `engine/shaders/` | GLSL shaders, compiled to SPIR-V and MSL at build time |
| `iso/` | `phantasm_iso`: optional isometric layer (grid projection, floor levels, picking) |
| `sandbox/` | Demo and stress test for the core |
| `tests/` | Catch2 unit tests |
| `benchmarks/` | Google Benchmark microbenchmarks for hot paths |
| `docs/adr/` | Architecture decision records |

## Architecture decisions

| ADR | Decision |
|---|---|
| [0001](docs/adr/0001-graphics-api-sdl-gpu.md) | SDL_GPU as the graphics API |
| [0002](docs/adr/0002-shader-pipeline.md) | GLSL → SPIR-V / MSL at build time |
| [0003](docs/adr/0003-image-loading-stb.md) | stb_image for image decoding |
| [0004](docs/adr/0004-isometric-first-general-core.md) | Isometric-first, genre-neutral core |
| [0005](docs/adr/0005-module-layout.md) | One repo, separate core and isometric libraries |
| [0006](docs/adr/0006-demos-in-separate-repository.md) | Engine repo is code-only; demos in a separate repo |

## Roadmap

- [x] M0: toolchain, CMake, tests
- [x] M1: window, loop, input
- [x] M2: SDL_GPU clear screen
- [x] M3: textured sprite
- [x] M4: camera + batched sprites
- [ ] M5: a small playable isometric game
- [ ] CI on macOS and Windows, v0.1 release

## License

[MIT](LICENSE). Third-party: SDL3 (zlib), stb_image and stb_rect_pack (public domain / MIT), Catch2 (BSL-1.0),
Google Benchmark (Apache-2.0).
