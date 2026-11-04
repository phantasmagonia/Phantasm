#!/usr/bin/env bash
set -euo pipefail
bin="${1:?build dir}/bin/compare_entity_flags"
echo 'mode,entities,toggled,frames,median_ms'
for run in 1 2 3; do
  for mode in entt archetype sidecar; do
    "$bin" "$mode"
  done
done
