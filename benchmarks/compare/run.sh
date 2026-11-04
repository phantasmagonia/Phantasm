#!/usr/bin/env bash
# Runs every engine on every workload, 3 times, interleaved to spread thermal effects.
# Usage: benchmarks/compare/run.sh <build-dir> [offscreen|window] [sprites|iso]   (prints CSV)
set -euo pipefail
bin="${1:?build dir}/bin"
mode="${2:-offscreen}"
suite="${3:-sprites}"
if [[ "$suite" == "iso" ]]; then
  counts=(1000 10000 50000)   # characters on a 160x160 tile map
  scenarios=(iso)
else
  counts=(10000 50000 100000 200000)
  scenarios=(plain sorted)
fi
echo "engine,scenario,mode,count,frames,avg_ms,p50_ms,p99_ms,throughput_ms"
for run in 1 2 3; do
  for count in "${counts[@]}"; do
    for scenario in "${scenarios[@]}"; do
      for engine in phantasm raylib sfml sdl_renderer; do
        "$bin/compare_$engine" "$count" "$scenario" "$mode" 2>/dev/null | grep "^$engine,"
      done
    done
  done
done
