# ADR 0003: stb_image for image decoding

- Status: Accepted
- Date: 2026-11-04

## Context
Textures need PNG (and likely JPEG) decoding.

## Options
1. **stb_image**: single header, public domain / MIT, fast, no dependencies.
2. **SDL3_image**: official SDL add-on, more formats, larger dependency.

## Decision
stb_image, pinned to a commit, built with PNG and JPEG only, decoding from memory.
It is used only inside `engine/src/image.cpp` behind `decode_image()`.

## Consequences
- Tiny dependency, trivial to build on every platform.
- stb_image is not hardened against malicious files; fine for game-shipped assets,
  not for untrusted user uploads.
- Swapping decoders later only touches `image.cpp`.
