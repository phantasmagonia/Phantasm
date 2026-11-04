#!/usr/bin/env bash
# Runs every engine on every workload, 3 times, interleaved to spread thermal effects.
# Usage: benchmarks/compare/run.sh <build-dir>   (prints CSV)
set -euo pipefail
bin="${1:?build dir}/bin"
echo "engine,scenario,sprites,frames,avg_ms,p50_ms,p99_ms"
for run in 1 2 3; do
  for count in 10000 50000 100000 200000; do
    for scenario in plain sorted; do
      for engine in phantasm raylib sfml sdl_renderer; do
        "$bin/compare_$engine" "$count" "$scenario" 2>/dev/null | grep "^$engine,"
      done
    done
  done
done
