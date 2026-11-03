# ADR 0001: Use SDL_GPU as the graphics API

- Status: Accepted
- Date: 2026-11-04

## Context
Phantasm must run on macOS (Apple Silicon) and Windows. The platform layer already
uses SDL3 for windowing and input. We need a GPU API for 2D sprite rendering.

## Options
1. **SDL_GPU** — ships with SDL3. Modern explicit API (command buffers, render passes,
   pipelines) over Metal (macOS), Direct3D 12 / Vulkan (Windows).
2. **OpenGL 4.1** — simpler, many tutorials, but deprecated on macOS and frozen at 4.1.
3. **Raw Metal / D3D12 / Vulkan** — maximum control, but one backend per platform.

## Decision
Use SDL_GPU.

## Consequences
- No new dependency; same zlib license as SDL3.
- Native modern backend on each platform; no deprecated API on macOS.
- Shaders must be supplied per backend (MSL / DXIL / SPIR-V); see ADR 0002.
- Fewer learning resources than OpenGL.
