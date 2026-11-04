# AGENTS.md

Guidance for AI coding agents working on Phantasm, an open-source C++20 2D game engine
on SDL3 + SDL_GPU, built to be the complete and fastest isometric engine.

## Build and test

```sh
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
./build/debug/bin/sandbox
```

Requires `glslc` and `spirv-cross` on PATH (shaders compile at build time).
Executables are written to `build/<preset>/bin/`, with compiled shaders in its `shaders/`
subdirectory. The sandbox generates textures in code; no asset copying is needed.

- After changes to C++ code, headers, shaders, tests, or build configuration, build with
  `cmake --build --preset debug` and run `ctest --preset debug --output-on-failure`
  before committing. Run `cmake --preset debug` first after build configuration or dependency changes.
- For bug fixes, add a regression test where practical and confirm it fails before the fix.
  Focused tests help during development; run the full debug suite after the final code change.
- For rendering or window changes, also run the debug sandbox when a display and GPU are available.
- Documentation-only changes do not require rebuilding or rerunning tests.
- Report the checks actually run and their results, including any checks that could not run.
  Release benchmarks and CI do not replace local debug tests.

## Performance work

Performance claims must come from benchmarks, with hardware stated. Use the release preset:

```sh
cmake --preset release && cmake --build --preset release
./build/release/bin/phantasm_benchmarks   # add --benchmark_filter=<regex> to narrow
./build/release/bin/sandbox --bench       # vsync off, prints fps / p50 / p99 / render CPU
```

Measure before and after every optimization; include both numbers in the commit message.

## Scale principles

Phantasm targets large isometric games. New code must not cap what a big game can do:

- **Data-oriented:** hot data in contiguous arrays, processed in bulk; no per-object virtual
  calls or pointer chasing in per-frame loops.
- **Handles, not pointers,** for engine-owned objects that can be destroyed (generational ids).
- **No per-frame heap churn** in hot paths: reuse buffers; prefer frame/pool allocation.
- **Parallel by design:** systems expose work that can run on the task pool; no hidden global
  mutable state.
- **Proportional to what's visible or active,** not to world size (chunks, culling, streaming).
- **Large worlds:** keep precision in mind (chunk-local or camera-relative coordinates for far
  positions).
- **Measured:** every scale claim backed by a benchmark at large counts.

Decided: **entity storage is Phantasm's own archetype store** (no EnTT). Prototype benchmark,
MacBook Air M1 8 GB, release, own store vs. EnTT v3.16: update 1M 1.35 vs 1.50-2.00 ms; render
extraction 1M 1.59 vs 3.10 ms; mixed iso world (100k movers) 291 vs 520 us; spawn/despawn churn
1M 2.03 vs 3.42 ms; memory 47.0 vs 56.5 B/entity. EnTT won add/remove of a component (0.31 vs
3.35 ms for 1% of 1M per frame), so frequently toggled tags live outside archetypes.

Open architecture decisions (benchmark before choosing): job system with dependencies, render
graph for lighting/fog passes, asset pipeline with async loading by handle.

Apply these checks when designing or reviewing a subsystem:

- **Entity storage:** use the archetype store; keep per-frame hot paths as bulk column loops.
  Measure bulk iteration, creation/destruction, component changes, and memory when changing
  it. Contiguous storage improves locality; it does not eliminate cache misses by itself.
- **Resource references:** validate generational handles on lookup and test stale references
  after slot reuse. Define ownership and lifetime for textures, entities, and resident chunks.
  Saves and networking need persistent IDs mapped to runtime handles.
- **Scheduling:** evaluate task dependencies and work stealing before multiple subsystems
  rely on cross-system parallel execution. The current blocking `parallel_for` is not a
  dependency scheduler; do not nest calls on the same pool.
- **Memory:** measure allocations in frame loops before claiming allocation-free behavior.
  Frame and pool storage must have explicit lifetimes, including tasks that outlive a frame.
- **World precision:** evaluate chunk-local simulation and camera-relative rendering before
  expanding world coordinates. Test distant positions and chunk boundaries, including physics
  queries and rendering conversions.
- **Simulation:** keep fixed simulation steps separate from rendering and use previous/current
  state with `FixedStep::alpha()` for interpolation where needed. Fixed steps alone do not
  guarantee deterministic physics, replays, or lockstep networking; verify those separately.
- **Rendering and assets:** declare pass inputs, outputs, and resource lifetimes when adding
  lighting, fog, or post-processing. Asset loading must distinguish CPU preparation from GPU
  upload and publish results through validated handles. GPU culling/sorting requires measured
  evidence that the CPU path is a bottleneck.
- **Profiling:** expose useful per-system timings alongside frame/render statistics. Record
  hardware, workload, and measurement boundaries for every performance comparison.

## Layout and dependency rules

- `engine/` → library `phantasm` (core). Must never depend on `iso/` or any game.
- `iso/` → library `phantasm_iso`, links `phantasm`.
- `physics/` → library `phantasm_physics` (opt-in via `PHANTASM_BUILD_PHYSICS`), links `phantasm`;
  Box2D is PRIVATE. Public physics API uses engine world units; convert to meters only inside.
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
