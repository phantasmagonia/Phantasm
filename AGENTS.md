# AGENTS.md

Guidance for AI coding agents working on Phantasm, an open-source C++20 2D game engine
on SDL3 + SDL_GPU with an optional isometric layer.

## Build and test

```sh
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
./build/debug/bin/sandbox
```

Requires `glslc` and `spirv-cross` on PATH (shaders compile at build time).
Executables, compiled shaders (`bin/shaders/`), and assets (`bin/assets/`) share `build/<preset>/bin/`.

## Performance work

Performance claims must come from benchmarks, with hardware stated. Use the release preset:

```sh
cmake --preset release && cmake --build --preset release
./build/release/bin/phantasm_benchmarks   # add --benchmark_filter=<regex> to narrow
./build/release/bin/sandbox --bench       # vsync off, prints fps / p50 / p99 / render CPU
```

Measure before and after every optimization; include both numbers in the commit message.

## Layout and dependency rules

- `engine/` → library `phantasm` (core). Must never depend on `iso/` or any game.
- `iso/` → library `phantasm_iso`, links `phantasm`.
- Games and demos live in the separate `phantasm-demo-projects` repository.
- No art files in this repository. Tests, benchmarks, and the sandbox generate textures in code
  (`Renderer::create_texture`).
- SDL is a PRIVATE dependency: public headers forward-declare SDL types and never include SDL.
- Third-party code comes in through `cmake/Dependencies.cmake` (FetchContent, pinned versions).

## Conventions

- C++20, warnings as errors (`-Wall -Wextra -Wpedantic -Werror`, `/W4 /WX`), applied to project targets only.
- RAII for every GPU/OS resource; resource types are non-copyable, move-only where needed.
  Class member declaration order encodes destruction order. Keep it correct.
- Pure logic (math, sorting, input state, timing) lives in SDL-free code and is unit tested
  with Catch2. Write tests first.
- Comments only for non-obvious "why"; no restating code.
- Conventional commits (`feat:`, `fix:`, `refactor:`, `docs:`, `test:`, `chore:`).

## SDL_GPU shader gotchas

- Binding sets follow SDL conventions: vertex samplers/storage set 0, vertex uniforms set 1,
  fragment samplers set 2, fragment uniforms set 3.
- spirv-cross assigns Metal `[[buffer(n)]]` slots by descriptor set, so a vertex storage buffer
  would collide with SDL's uniform slot 0. Sprite data therefore uses an instanced vertex buffer.
- After changing shaders, inspect `build/debug/bin/shaders/*.msl` for slot numbers.
