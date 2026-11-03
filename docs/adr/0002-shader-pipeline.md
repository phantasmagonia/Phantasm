# ADR 0002: GLSL shaders compiled to SPIR-V and MSL at build time

- Status: Accepted
- Date: 2026-11-04

## Context
SDL_GPU needs shaders in each backend's native format: SPIR-V (Vulkan), MSL (Metal),
DXIL (Direct3D 12). SDL_shadercross and DXC are not packaged for Homebrew and
building them pulls in an LLVM-sized toolchain.

## Decision
- Write shaders once in GLSL 450 (`engine/shaders/`).
- At build time: `glslc` → SPIR-V, then `spirv-cross --msl` → MSL (`cmake/Shaders.cmake`).
- Output goes next to the executables (`bin/shaders/`); nothing generated is committed.
- No DXIL for now: the device is created with SPIR-V | MSL only, so Windows uses Vulkan.

## Consequences
- Offline compilation: no runtime shader compiler, fast startup.
- Tools required to build: `glslc` and `spirv-cross` (Homebrew on macOS; Vulkan SDK on Windows).
- SPIR-V is the hub format: DXIL can be added later from the same SPIR-V
  (e.g. via SDL_shadercross) without rewriting shaders.
- Resource bindings must follow SDL_GPU's set conventions (vertex uniforms: set 1,
  fragment samplers: set 2).
