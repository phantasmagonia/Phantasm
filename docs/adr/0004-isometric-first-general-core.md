# ADR 0004: Genre-neutral core, isometric as the first-class target

- Status: Accepted
- Date: 2026-11-04

## Context
The engine should lean towards isometric games while staying usable for
top-down and side-view 2D games.

## Decision
- The core (platform, renderer, camera, batching, ECS, physics) stays genre-neutral
  and works in plain 2D world coordinates.
- Isometric support is an opt-in layer on top: projection helpers
  (world/grid ↔ screen), isometric tilemaps, and picking.
- Features isometric needs are built into the core from the start because they are
  cheap now and costly to retrofit:
  - per-sprite sort key (layer + depth), sorted before batching;
  - sprite pivot/origin (isometric sprites anchor at their feet, not top-left);
  - camera in world space, independent of projection.

## Consequences
- Isometric games get correct "walk behind" draw order by setting depth (e.g. from
  world y), without special engine code paths.
- Non-isometric games ignore depth/pivot (defaults: depth 0, pivot top-left).
- Sorting costs O(n log n) per frame; acceptable for thousands of sprites, measure in M4.
