# 0011: Fixed worker pool for independent world updates

## Status

Accepted.

## Context

The sandbox updates every moving object before rendering. Independent object ranges can be
updated concurrently, but starting threads each frame would add overhead and complicate
resource lifetime.

## Decision

`TaskPool` owns a fixed number of C++20 worker threads and runs a caller-provided function
over disjoint index ranges. The calling thread also processes ranges and waits for workers
before returning. Worker exceptions return to the caller after all active ranges finish.
Calls on one pool are serialized; callbacks must synchronize any shared state. The pool lives
in the SDL-free core and does not perform GPU work.

The sandbox has a paired sequential/parallel scenario for one million moving sprites. The
parallel path uses three workers and 65,536-object ranges.

## Consequences

On a MacBook Air M1 (8 GB), a release CPU-only benchmark updated one million 16-byte objects
in 1.00 ms sequentially and 0.33 ms in parallel. Across three paired GPU sandbox runs, the
median update cost was 1.90 ms sequentially and 1.69 ms in parallel, with no consistent FPS
gain. The pool is useful for independent, sufficiently large workloads; multithreaded sorting
and GPU upload preparation remain separate work.
