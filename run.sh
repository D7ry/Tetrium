#!/usr/bin/env bash
set -euo pipefail

python extern/TetriumColor/scripts/validation/visualize_metamer_summary.py \
  --primaries extern/TetriumColor/measurements/2026-04-28/validation_2026-04-28_18-28-56/primaries \
  --measurements extern/TetriumColor/measurements/2026-04-28/validation_2026-04-28_18-28-56/validation_measurements \
  --metamers extern/TetriumColor/measurements/2026-04-28/validation_2026-04-28_18-28-56/display_validation_metamers.json \
  --plots-dir extern/TetriumColor/measurements/2026-04-28/validation_2026-04-28_18-28-56/metamer_summary_top10 \
  --n-observers 10 \
  --mode plate \
  --figure-version hyperobserver
