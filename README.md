# Phantasm

An open-source 2D game engine in C++20, built on SDL3 and SDL_GPU.
Isometric-first, with a genre-neutral core.

**Status:** early development. Camera + batched sprites (100k sprites, 1 draw call, ~96 fps release on M-series).

## Build

Requirements: CMake 3.26+, Ninja, a C++20 compiler, `glslc` and `spirv-cross`.

```sh
# macOS
brew install cmake ninja shaderc spirv-cross

cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/bin/sandbox          # optional: sprite count, e.g. 10000
```

Sandbox controls: WASD/arrows pan, Q/E zoom, 1/2/3 spawn 1k/10k/100k sprites,
Space cycles the background, Esc quits.

## Layout

| Path | Contents |
|---|---|
| `engine/` | `phantasm`: genre-neutral core (platform, input, renderer, camera, sprites) |
| `iso/` | `phantasm_iso`: optional isometric layer (grid projection, floor levels, picking) |
| `sandbox/` | Demo app for trying engine features |
| `tests/` | Catch2 unit tests |
| `docs/adr/` | Architecture decision records |
