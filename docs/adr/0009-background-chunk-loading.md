# 0009: Background chunk loading with main-thread publication

## Status

Accepted.

## Context

Synchronous chunk loading can stall a frame even with a small load budget. Games may need to
read or generate chunk data while rendering continues, while GPU resource creation remains on
the game thread.

## Decision

`AsyncChunkStream<T>` runs a game-provided loader on one worker. The game thread updates the
desired camera area and limits outstanding requests. The worker skips queued chunks that are
no longer desired; an in-progress load finishes, but its result is discarded if the camera has
moved away. `poll()` publishes completed data on the game thread. Failures are reported with
their exception and paused until the game calls `retry()`.

The loader may perform file I/O and CPU preparation but must not call SDL_GPU or access
game-thread-only state. Destruction joins the worker and may wait for an active load.

## Consequences

The game loop does not run the loader during normal updates. The API owns one worker
per stream, not a general thread pool. CPU and memory costs of constructing, publishing, and
rendering streamed worlds still need end-to-end measurements.
