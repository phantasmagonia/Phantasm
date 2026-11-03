# Phantasm

**A performance-first, open-source 2D game engine in modern C++20, built to be the core of large
isometric games.** Genre-neutral at its heart, with an optional isometric layer for big,
multi-level worlds. Built on SDL3 and SDL_GPU, MIT-licensed.

> **Status:** early development (pre-0.1). The core renderer and isometric math work and are
> tested; gameplay systems (physics, tilemaps, audio, editor) are on the roadmap. Developed and
> measured on macOS (Metal). Windows and Linux (Vulkan) are targets but not yet tested in CI.

**Contents:** [Why Phantasm](#why-phantasm) · [Optimization work](#optimization-work) ·
[Scalability](#scalability) · [Performance](#performance) · [Features](#features) ·
[Build](#build) · [Example](#minimal-example) · [Roadmap](#roadmap)

## Why Phantasm

- **Fast by design, proven by benchmarks.** Every performance claim comes from committed,
  reproducible benchmarks with the hardware stated. No optimization is merged without
  before/after numbers in its commit message.
- **Built for large isometric worlds.** Depth-sorted rendering that stays batched, floor
  levels, tile picking, and off-screen culling are in the core design, not bolted on later.
- **Modern GPU path.** SDL_GPU on native backends (Metal, Vulkan; Direct3D 12 planned),
  instanced rendering, shaders compiled ahead of time with no runtime shader compiler.
- **Small, readable C++20.** RAII ownership everywhere, warnings-as-errors, no hidden global
  state in the public API, and a test suite written alongside the code.
- **Genre-neutral core.** Top-down and side-view games use the core alone; the isometric layer
  is a separate, opt-in library.
- **Truly open.** MIT license, permissive dependencies only (zlib, MIT, public domain,
  Apache-2.0, BSL-1.0). No copyleft code, no royalties.

## Optimization work

Each step was measured before and after on a MacBook Air (M1, 8 GB RAM), release build.

| Optimization | What it does | Measured effect |
|---|---|---|
| **Instanced batching** | Sprites become per-instance data in one GPU buffer; the GPU builds quads from the vertex index | 1,000,000 sprites in **1 draw call** |
| **Texture atlas** | Packs many images into one texture (stb_rect_pack) so depth sorting can't break batches | 100k sprites over 16 images: **93,664 → 1 draw call**, render CPU **28.5 → 9.3 ms** |
| **Radix sort** | Stable LSD radix sort on a packed 64-bit key (layer, depth, texture) instead of a comparison sort | 100k sprites: **9.1 → 1.3 ms (7.1x)**; 1M: **104 → 24.7 ms** |
| **Off-screen culling** | Sprites outside the camera view are dropped before sorting and upload | 1M-sprite world viewed up close: sort/upload **31.3 → 0.43 ms** |

## Scalability

How Phantasm keeps frame cost proportional to what is *visible*, not to world size:

| Layer | Approach | Status |
|---|---|---|
| Draw calls | Instancing + atlases: one draw per texture run, usually one per scene | Done |
| Sorting | Linear-time radix sort; skips key bytes that don't vary | Done |
| Off-screen work | Camera culling before sort and upload | Done |
| World size | Chunked spatial partitioning so games only submit chunks the camera sees; chunk streaming for huge maps | Next |
| CPU cores | Multithreaded sorting, upload, and chunk updates | Planned |
| Physics | Box2D v3 (MIT; multithreaded, SIMD) behind an engine interface, benchmarked before adoption | Planned |
| GPU-driven rendering | Culling and sorting on the GPU for multi-million-object scenes | Researching |

## Performance

All numbers: **MacBook Air (M1, 8 GB RAM)**, release build. Run the benchmarks yourself with
the commands below.

**End-to-end** (`sandbox --bench`, vsync off, 240 measured frames).
*Submit* = the game's `draw_sprite` calls + culling + `end_frame`; *end_frame* = sort, upload,
and command recording. These CPU timings are the reliable numbers; FPS and p99 include
macOS compositor effects.

| Sprites | Textures | Zoom | Visible | FPS | Submit | end_frame | Draw calls |
|---:|---|---:|---:|---:|---:|---:|---:|
| 10,000 | 1 | 0.5 | 10,000 | 649 | 0.44 ms | 0.29 ms | 1 |
| 100,000 | 1 | 0.5 | 100,000 | 185 | 2.65 ms | 1.89 ms | 1 |
| 1,000,000 | 1 | 0.5 | 1,000,000 | 18 | 38.2 ms | 31.3 ms | 1 |
| 100,000 | 16 separate | 0.5 | 100,000 | 50 | 19.9 ms | 19.2 ms | 93,699 |
| 100,000 | 16 in one atlas | 0.5 | 100,000 | 191 | 2.57 ms | 1.78 ms | 1 |
| 1,000,000 | 16 in one atlas | 4.0 | 27,850 | 31 | 14.6 ms | 0.43 ms | 1 |

The last row is a large world viewed up close: culling leaves ~3% of sprites, and the engine's
sort/upload cost drops to 0.43 ms. What remains is the game submitting a million objects per
frame, which spatial partitioning (chunks) will remove.

**CPU microbenchmarks** (`phantasm_benchmarks`), sorting + grouping, random depth:

| Sprites | `std::stable_sort` | Radix sort (current) | Speed-up |
|---:|---:|---:|---:|
| 10,000 | 0.62 ms | 0.11 ms | 5.7x |
| 100,000 | 9.1 ms | 1.3 ms | 7.1x |
| 1,000,000 | 104 ms | 24.7 ms | 4.2x |

**Texture atlases** (`build_atlas`) keep depth-sorted scenes in one draw call no matter how
many images they use. **Off-screen culling** happens in `draw_sprite`, before sorting.
**Next:** chunked spatial partitioning for large worlds, then multithreading.

Run them yourself:

```sh
cmake --preset release && cmake --build --preset release
./build/release/bin/phantasm_benchmarks     # CPU microbenchmarks (Google Benchmark)
./build/release/bin/sandbox --bench         # GPU end-to-end, vsync off
```

Close other apps first; background load skews results.

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

- **Benchmarks:** Google Benchmark microbenchmarks plus an end-to-end GPU benchmark mode.

Planned: chunked worlds, physics, isometric tilemaps, wall cutaway, line of sight, lighting,
animation, audio, an editor (Dear ImGui), and Direct3D 12. See [Roadmap](#roadmap).

## Build

Requirements: CMake 3.26+, Ninja, a C++20 compiler (Clang 17+, MSVC 2022), `glslc`, and `spirv-cross`.
SDL3, stb, Catch2, and Google Benchmark are downloaded automatically by CMake.

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
- [x] Performance pass 1: benchmarks, texture atlas, radix sort, culling
- [ ] Chunked spatial partitioning and streaming for large worlds
- [ ] Physics (Box2D v3) and collision
- [ ] Multithreaded sort, upload, and world updates
- [ ] M5: a small playable isometric game ([demos](https://github.com/hacengeloglu-blip/phantasm-demo-projects))
- [ ] CI on macOS, Windows, and Linux; Direct3D 12 backend
- [ ] v0.1 release

## License

[MIT](LICENSE). Third-party: SDL3 (zlib), stb_image and stb_rect_pack (public domain / MIT), Catch2 (BSL-1.0),
Google Benchmark (Apache-2.0).
