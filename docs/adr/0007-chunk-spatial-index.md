# 0007: World-space chunk index in the core

## Status

Accepted.

## Context

The renderer rejects off-screen sprites before sorting, but a game that submits every object
still pays to visit its whole world each frame. Isometric projection is optional, so visibility
lookup must work in world coordinates without depending on the isometric library.

## Decision

`ChunkIndex` stores world-space axis-aligned bounds under game-owned integer IDs. An object is
registered in every chunk its bounds overlap. A query visits chunks touched by the view, checks
exact rectangle intersection, deduplicates IDs, and returns IDs in ascending order. Games own
objects and call `Renderer::draw_sprite` only for query results. Moving objects use `upsert`;
deleted objects use `erase`.

Each chunk bucket stores an object's bounds beside its ID. This trades memory for fewer random
lookups during dense visibility queries.

## Consequences

Visibility lookup scales with chunks and objects near the view in the measured case. Objects
that span many chunks use more memory and cost more to update. Construction and movement costs
are outside the current visibility benchmark. The sandbox now compares linear and indexed
submission for a static world; moving-object index updates and background chunk loading are
outside that GPU comparison.
