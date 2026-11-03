# ADR 0006: Engine repository is code-only; demos live in a separate repository

- Status: Accepted (supersedes the `games/` row of ADR 0005)
- Date: 2026-11-04

## Context
Game art and game code do not belong in the engine's history: art bloats the
repository, carries its own licenses, and changes for reasons unrelated to the engine.
Established engines split this way (e.g. an engine repository plus a separate
demo-projects repository).

## Decision
- `Phantasm` contains only the engine (`engine/`, `iso/`), tests, benchmarks,
  the sandbox, and docs. No art files.
- Tests, benchmarks, and the sandbox use textures generated in code.
- Demos and example games live in `phantasm-demo-projects`, which consumes Phantasm
  through CMake FetchContent pinned to a commit. Art decisions and licenses live there.
- When Phantasm is included as a subproject, its tests, benchmarks, and sandbox are
  off by default (`PROJECT_IS_TOP_LEVEL`).

## Consequences
- Cloning the engine is small and fast; its license is uniform (MIT).
- Engine changes that break demos are caught only when the demo repository updates its
  pinned commit; CI for demos will cover this later.
- For local development, the demo repository can point at a local engine checkout via
  `-DFETCHCONTENT_SOURCE_DIR_PHANTASM=<path>`.
