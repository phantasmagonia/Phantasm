# Phantasm

An open-source 2D game engine in C++20, built on SDL3 and SDL_GPU.
Isometric-first, with a genre-neutral core.

**Status:** early development.

## Build

Requirements: CMake 3.26+, Ninja, a C++20 compiler, `glslc` and `spirv-cross`.

```sh
# macOS
brew install cmake ninja shaderc spirv-cross

cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/bin/sandbox
```

## Layout

| Path | Contents |
|---|---|
| `engine/` | The `phantasm` library (platform, input, renderer) and its shaders |
| `sandbox/` | Demo app for trying engine features |
| `tests/` | Catch2 unit tests |
| `docs/adr/` | Architecture decision records |
