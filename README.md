# Phantasm

**A performance-first, open-source 2D game engine in modern C++20, built to be the core of large
isometric games.** Genre-neutral at its heart, with an optional isometric layer for big,
multi-level worlds. Built on SDL3 and SDL_GPU, licensed under GPLv3 only.

> **Status:** early development (pre-0.1). The core renderer and isometric math work and are
> tested; rigid-body physics (Box2D) is available in an early form; audio and an editor are
> not implemented. Development and measurements have
> been on macOS (Metal). Builds and unit tests pass in CI on macOS, Windows, and Linux;
> GPU runtime behavior on Windows and Linux has not been verified yet.

> **AI use:** AI tools have been used to help design, write, test, and document Phantasm, and
> will continue to be used. Build, test, and benchmark results are reported only when verified.

**Contents:** [Why Phantasm](#why-phantasm) · [Optimization work](#optimization-work) ·
[Scalability](#scalability) · [Performance](#performance) · [Features](#features) ·
[Build](#build) · [Example](#minimal-example) · [License](#license)

## Why Phantasm

- **Fast by design, proven by benchmarks.** Every performance claim comes from committed,
  reproducible benchmarks with the hardware stated. No optimization is merged without
  before/after numbers in its commit message.
- **Built for large isometric worlds.** Depth-sorted rendering that stays batched, floor
  levels, tile picking, and off-screen culling are in the core design, not bolted on later.
- **Modern GPU path.** SDL_GPU on native backends (Metal and Vulkan),
  instanced rendering, shaders compiled ahead of time with no runtime shader compiler.
- **Small, readable C++20.** RAII ownership everywhere, warnings-as-errors, no hidden global
  state in the public API, and a test suite written alongside the code.
- **Genre-neutral core.** Top-down and side-view games use the core alone; the isometric layer
  is a separate, opt-in library.
- **Open with copyleft.** Phantasm is GPLv3-only: distributed engine derivatives stay under
  GPL terms. People can build and sell games with it, subject to the GPL's requirements for
  a distributed game linked to the engine. Dependencies retain their own permissive licenses.

## Optimization work

Each step was measured before and after on a MacBook Air (M1, 8 GB RAM), release build.

> **About the hardware:** all numbers come from an entry-level, fanless MacBook Air
> (M1, 8 GB RAM), which can throttle under sustained load. Faster CPUs and GPUs should
> produce better results: sorting, culling, and submission still depend mostly on single-core
> speed, while independent world updates can use more CPU cores.
> If you run the benchmarks on other hardware, results are welcome in an issue.

| Optimization | What it does | Measured effect |
|---|---|---|
| **Instanced batching** | Sprites become per-instance data in one GPU buffer; the GPU builds quads from the vertex index | 1,000,000 sprites in **1 draw call** |
| **Texture atlas** | Packs many images into one texture (stb_rect_pack) so depth sorting can't break batches | 100k sprites over 16 images: **93,664 → 1 draw call**, render CPU **28.5 → 9.3 ms** |
| **Radix sort** | Stable LSD radix sort on a packed 64-bit key (layer, depth, texture) instead of a comparison sort | 100k sprites: **9.1 → 1.3 ms (7.1x)**; 1M: **104 → 24.7 ms** |
| **Off-screen culling** | Sprites outside the camera view are dropped before sorting and upload | 1M-sprite world viewed up close: sort/upload **31.3 → 0.43 ms** |
| **Chunk index** | Games query world-space chunks before submitting sprites; bucket-local bounds avoid extra lookups | Static 1M-sprite world, 27,838 visible: submit **15.68 → 6.85 ms (2.3x)** |
| **Parallel world updates** | A reusable worker pool updates independent object ranges | 1M lightweight objects: **1.00 → 0.33 ms (3.0x)** CPU-only; moving sandbox world: **1.90 → 1.69 ms** median update CPU |

## Scalability

Rendering work depends on visible sprites when games use the chunk index before submission.
Updating moving objects and maintaining their index entries still costs work proportional to
the objects updated; streaming and the task pool help manage that work.

| Layer | Current approach |
|---|---|
| Draw calls | Instancing + atlases: one draw per texture run, usually one per scene |
| Sorting | Linear-time radix sort; skips key bytes that don't vary |
| Off-screen work | Camera culling before sort and upload |
| World size | Chunk index returns IDs of objects intersecting the camera view |
| Chunk residency | Load nearby game-owned chunks with a per-update budget; release departed chunks |
| Background loading | One worker loads game-owned chunk data; completed chunks join the world on the game thread |
| CPU cores | Fixed worker pool for independent world-update ranges |
| Physics | Box2D v3 behind `phantasm_physics`; fixed steps; resting bodies sleep and cost almost nothing |

## Performance

All numbers: **MacBook Air (M1, 8 GB RAM)**, release build; expect better results on faster
hardware (see the note under [Optimization work](#optimization-work)). Run the benchmarks
yourself with the commands below.

These are historical measurements from the implementation work, not a fresh benchmark of
every commit. In particular, the streaming and chunk-query figures predate the later queue
reprioritization and traversal validation fixes.

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

The last row uses moving sprites and predates indexed submission: culling leaves ~3% visible,
but the game still visits a million objects each frame. The separate static-world comparison
below uses the same generated sprites for both paths and skips movement in both cases.

**Static-world GPU comparison** (`sandbox --bench-static`, vsync off, 240 measured frames):
1,000,000 sprites in one atlas, zoom 4.0, 27,838 visible, 256-unit chunks. Index construction
is outside the measured frames. *Submit* includes visibility lookup, sprite submission, and
`end_frame`; it excludes world updates and background chunk loading.

| Submission path | Visible | Submit CPU | end_frame CPU | Draw calls |
|---|---:|---:|---:|---:|
| Linear scan | 27,838 | 15.68 ms | 0.45 ms | 1 |
| Chunk index | 27,838 | 6.85 ms | 0.56 ms | 1 |

On this MacBook Air M1 (8 GB), indexed submission reduced CPU submit time by **2.3x**. FPS
was 31 for the scan and 33 for the index; macOS compositor timing also affects those figures.

**Streamed-world GPU comparison** (`phantasm_stream_bench`, vsync off): procedural 256-unit
chunks with 8,192 sprites each; one-chunk prefetch margin; camera advances 32 units per frame.
Three paired runs of 240 measured frames after 30 warmup frames on the MacBook Air M1 (8 GB).
Both modes completed 148 chunk loads and averaged 7,609 visible sprites. The synchronous mode
loads up to two chunks per frame; background mode allows four outstanding requests and uses one
worker. Chunk generation is CPU-only; this benchmark does not measure disk I/O.

| Loading mode | Submit CPU mean | Submit CPU p99 | Resident chunks, mean |
|---|---:|---:|---:|
| Synchronous | 0.77 ms in all three runs | 0.97–0.99 ms | 16.2 |
| Background | 0.64–0.74 ms | 0.85–0.95 ms | 16.0 |

These are observed ranges for this procedural scene. Whole-frame p99 varied between 16 and
18 ms in both modes, so this run does not establish a general frame-rate gain.

**Moving-world update comparison** (`sandbox --bench-parallel`, vsync off): one million moving
sprites in an atlas, zoom 4.0, around 27,850 visible. Three paired runs of 240 measured frames
after 30 warmup frames on the MacBook Air M1 (8 GB). Parallel mode uses three worker threads
plus the game thread with 65,536-object ranges.

| Update mode | Update CPU mean across runs | Submit CPU mean across runs |
|---|---:|---:|
| Sequential | 1.87–1.91 ms (median 1.90) | 14.93–15.31 ms |
| Parallel | 1.62–1.73 ms (median 1.69) | 14.58–15.09 ms |

The update loop improved by **1.12x** at the median; frame rate stayed around 28–30 FPS in
both modes. The separate CPU-only `BM_SequentialWorldUpdate` / `BM_ParallelWorldUpdate`
benchmark uses 16-byte objects and excludes rendering: one million objects measured
**1.00 → 0.33 ms (3.0x)** with the same three-worker pool.

**Chunk query microbenchmark** (MacBook Air M1, 8 GB, release): a regular grid of 16×16
objects at 32-unit spacing, 256-unit chunks, and a 1,024×1,024 view containing 1,024 objects.
The timings measure visibility lookup only; index construction and rendering are excluded.

| World objects | Linear scan | Chunk query | Speed-up |
|---:|---:|---:|---:|
| 102,400 | 0.140 ms | 0.047 ms | 3.0x |
| 1,048,576 | 1.40 ms | 0.041 ms | 34x |

A dense-world query benchmark (`BM_DenseChunkQuery`, 1M sprites over a 2,400×1,400-unit
world) measured **12.1 → 3.13 ms** for 256-unit chunks after storing bounds in each bucket.
The separate dense linear scan measured 8.06 ms. These are lookup-only CPU timings on the same
MacBook Air M1 (8 GB); the GPU comparison above includes submission and sorting.

**CPU microbenchmarks** (`phantasm_benchmarks`), sorting + grouping, random depth:

| Sprites | `std::stable_sort` | Radix sort (current) | Speed-up |
|---:|---:|---:|---:|
| 10,000 | 0.62 ms | 0.11 ms | 5.7x |
| 100,000 | 9.1 ms | 1.3 ms | 7.1x |
| 1,000,000 | 104 ms | 24.7 ms | 4.2x |

**Texture atlases** (`build_atlas`) allow images sharing one atlas to batch into one texture
run. Multiple atlases or textures can still require multiple draw calls. **Off-screen culling**
happens in `draw_sprite`, before sorting.

Run them yourself:

```sh
cmake --preset release && cmake --build --preset release
./build/release/bin/phantasm_benchmarks     # CPU microbenchmarks (Google Benchmark)
./build/release/bin/sandbox --bench         # GPU end-to-end, vsync off
./build/release/bin/sandbox --bench-static  # static world: linear vs chunk index
./build/release/bin/sandbox --bench-parallel # moving world: sequential vs parallel updates
./build/release/bin/phantasm_stream_bench              # synchronous chunk generation
./build/release/bin/phantasm_stream_bench --background # background chunk generation
```

Close other apps first; background load skews results.

**Physics** (`phantasm_benchmarks`, MacBook Air M1 8 GB, release, single-threaded Box2D,
60 Hz steps, 4 sub-steps):

| Scenario | Bodies | Time per step |
|---|---:|---:|
| Top-down crowd (circles, no gravity, moving) | 10,000 | 1.46 ms |
| Top-down crowd | 20,000 | 3.56 ms |
| Pile collapse (boxes falling and stacking) | 10,000 | 0.94 ms |
| Resting pile (bodies asleep) | 10,000 | ~0 ms |

Box2D's own multithreading is not wired up yet; at these costs it is not needed to fit a
16.7 ms frame.

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

- **World visibility:** chunk index, bounded chunk residency, and background chunk loading.
- **World updates:** reusable task pool for independent ranges, with worker errors returned
  to the caller.
- **Physics (`phantasm_physics`, opt-in):** Box2D v3 rigid bodies (boxes, circles, static /
  kinematic / dynamic, gravity or top-down), in engine world units, with handles that stay safe
  after a body is destroyed. `FixedStep` turns frame times into fixed simulation steps.
- **Benchmarks:** Google Benchmark microbenchmarks plus end-to-end GPU benchmark modes.

## Build

Requirements: CMake 3.26+, Ninja, a C++20 compiler (Clang 17+, GCC 14+, or MSVC 2022),
`glslc`, and `spirv-cross`.
SDL3, stb, Box2D, Catch2, and Google Benchmark are downloaded automatically by CMake.

```sh
# macOS
brew install cmake ninja shaderc spirv-cross

cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/bin/sandbox          # optional: sprite count, e.g. 10000
```

For code, shader, test, or build changes, build the debug preset and run
`ctest --preset debug --output-on-failure` before committing. Rendering changes also need
a debug sandbox check when a display and GPU are available. Documentation-only changes
do not require rerunning tests. See [AGENTS.md](AGENTS.md) for the verification workflow.

On Ubuntu 24.04, the CI package list and commands are:

```sh
sudo apt-get update
sudo apt-get install -y cmake g++-14 glslc spirv-cross ninja-build \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
  libxi-dev libxss-dev libxtst-dev libxkbcommon-dev
CC=gcc-14 CXX=g++-14 cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

See [SDL's Linux build dependencies](https://wiki.libsdl.org/SDL3/README-linux)
for additional optional platform features.

On Windows, use a Visual Studio 2022 Developer Command Prompt and install the
[Vulkan SDK](https://vulkan.lunarg.com/) for `glslc` and `spirv-cross`. Then run the same
configure, build, and test commands. The sandbox needs a working SDL_GPU backend and display.

Sandbox controls: WASD/arrows pan, Q/E zoom, 1/2/3 spawn 1k/10k/100k sprites,
Space cycles the background, T cycles texture modes, Esc quits.

## Minimal example

This draws a generated white square and moves it with the right arrow key; no art files are needed.

```cpp
#include <phantasm/application.hpp>

#include <array>
#include <cstddef>

constexpr std::array<std::byte, 4> white_pixel{
    std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};

class Game final : public phantasm::Application {
public:
    Game() : hero_{renderer().create_texture(1, 1, white_pixel)} {}

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

For a large world, keep objects in game-owned storage and index their world bounds. Update the
index when an object moves, then submit only IDs returned for the camera view:

```cpp
phantasm::ChunkIndex index{256.0F};
index.upsert(id, phantasm::sprite_world_rect(sprites[id]));

for (const auto visible_id : index.query(renderer.visible_rect())) {
    renderer.draw_sprite(textures[visible_id], sprites[visible_id]);
}
```

The game owns the sprites and textures; `ChunkIndex` stores bounds and IDs only. Call
`visible_rect()` after `begin_frame()` to get the current camera view.
Include `<phantasm/chunk_index.hpp>` for this snippet. Index operations reject unsupported
coordinates with `std::invalid_argument` and bounds spanning more than 1,000,000 chunks with
`std::length_error`. A rejected geometry update preserves the object's previous bounds.

For worlds whose chunks are created on demand, `ChunkStream<T>` holds chunk data near the camera.
Its loader runs synchronously, so keep the per-update budget small or supply prepared data:

```cpp
phantasm::ChunkStream<ChunkData> world{256.0F, 1}; // one-chunk prefetch margin
world.update(camera_view, 2, [](phantasm::ChunkCoord chunk) {
    return load_chunk(chunk); // game-provided loader
});
for (const auto& [coord, data] : world.loaded()) {
    draw_chunk(coord, data);
}
```

`AsyncChunkStream<T>` uses the same camera-driven residency with one background worker:

```cpp
phantasm::AsyncChunkStream<ChunkData> world{256.0F, 1, load_chunk};
world.update(camera_view, 4); // at most four requests queued, running, or awaiting poll
world.poll();                 // move finished chunks into the game-thread resident map
for (const auto& [coord, data] : world.loaded()) {
    draw_chunk(coord, data);
}
```

The loader must be thread-safe and cannot create SDL_GPU resources. Check
`take_failures()` for load errors and call `retry(coord)` when a retry is appropriate.
Queued requests are ordered nearest the current view center first, including after camera
movement; active loads finish normally. Departed chunks and stale completed results are discarded.
The stream waits for an active load when destroyed; loaders should finish or support their own
cancellation. The procedural GPU benchmark above exercises both loading paths.

Include `<phantasm/chunk_stream.hpp>` or `<phantasm/async_chunk_stream.hpp>` for the respective
stream examples. `ChunkData`, `load_chunk`, and `draw_chunk` are supplied by the game.

## Demos

Example games live in [phantasm-demo-projects](https://github.com/phantasmagonia/phantasm-demo-projects),
which builds against a pinned Phantasm commit. This repository contains no art.

## Repository layout

| Path | Contents |
|---|---|
| `engine/` | `phantasm`: genre-neutral core (platform, input, renderer, camera, sprites) |
| `engine/shaders/` | GLSL shaders, compiled to SPIR-V and MSL at build time |
| `iso/` | `phantasm_iso`: optional isometric layer (grid projection, floor levels, picking) |
| `physics/` | `phantasm_physics`: optional rigid-body physics (Box2D) |
| `sandbox/` | Demo and stress test for the core |
| `tests/` | Catch2 unit tests |
| `benchmarks/` | Google Benchmark microbenchmarks for hot paths |

## License

Copyright (c) 2026 Halil Ahmet Çengeloğlu. Phantasm's project-owned code and documentation are
licensed under the [GNU General Public License, version 3 only](LICENSE) (`GPL-3.0-only`),
with no game-linking exception. If you distribute a game linked to Phantasm, the combined
program generally must be available under GPLv3 terms. Private development does not require
publishing your game.

GPLv3 **allows companies to use and sell** the engine under its terms; it does not reserve
engine sales to the author. Third-party components keep their own licenses: SDL3 (zlib),
stb_image and stb_rect_pack (public domain / MIT), Box2D (MIT), Catch2 (BSL-1.0), and
Google Benchmark (Apache-2.0).
