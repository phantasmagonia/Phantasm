# ADR 0005: One repository, separate libraries for core and isometric

- Status: Accepted; `games/` row superseded by ADR 0006
- Date: 2026-11-04

## Context
The engine should have a genre-neutral core plus an isometric layer (ADR 0004).
Options considered: a fork per flavor, separate repositories, or one repository
with separate libraries.

## Decision
One repository, separate CMake library targets:

| Target | Folder | Depends on |
|---|---|---|
| `phantasm` | `engine/` | SDL3, stb_image |
| `phantasm_iso` | `iso/` | `phantasm` |
| games | `games/<name>/` | `phantasm`, optionally `phantasm_iso` |
| `sandbox` | `sandbox/` | `phantasm` |

The core never depends on `phantasm_iso`; the link graph enforces this.

## Isometric scope
Target: large, multi-level isometric worlds. 2:1 diamond tiles, multiple floor
levels, wall cutaway/fade near the player, line of sight and fog of war, lighting,
and chunk-streamed maps. Built incrementally as games need them.

## Consequences
- One place for fixes, tests, and CI; no fork drift.
- Games that don't need isometric link only `phantasm`.
- Once the engine is versioned (v0.1+), games may move to their own repositories
  and consume a tagged engine release.
