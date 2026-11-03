# 0008: Bounded, game-owned chunk residency

## Status

Accepted.

## Context

Games need to create chunks near the camera and release distant ones without keeping the entire
world in memory. Chunk content and its storage format differ by game. The engine should manage
residency without taking ownership of a game's asset pipeline.

## Decision

`ChunkStream<T>` is a genre-neutral, synchronous resident set. A game supplies a loader that
returns chunk data. Each update removes chunks outside the view plus a configurable prefetch
margin, then loads up to a specified number of missing chunks, nearest the view center first.
It accepts move-only chunk data and retries a chunk if its loader throws. Empty views leave
residency unchanged, which avoids unloading the world when a window is minimized.

## Consequences

Games can bound the number of new chunks loaded per update, but a slow loader can still stall a
frame. Background I/O and preparation need a later async interface. Large views spanning more
than one million chunks are rejected rather than allocating an unbounded work list.
