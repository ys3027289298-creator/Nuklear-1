#!/usr/bin/env bash
# Reproducible validation for the property-table demo.
# Builds the repo's own GLFW/OpenGL2 entry, runs it headlessly on $DISPLAY,
# and leaves geometry, scroll and timing artifacts under output/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

: "${DISPLAY:=:0}"
export DISPLAY

make -C demo/property_table

mkdir -p output
demo/property_table/bin/property_table

# repeat several passes to characterize the single-frame upper bound under
# scheduler jitter; each pass rewrites the JSON artifacts, keep the max series
echo "pass,max_ms,avg_ms" > output/table_passes.csv
for pass in 1 2 3 4 5; do
    demo/property_table/bin/property_table >/dev/null
    max_ms=$(sed -n 's/.*"table_max_ms": \([0-9.]*\).*/\1/p' output/table_benchmark.json)
    avg_ms=$(sed -n 's/.*"table_avg_ms": \([0-9.]*\).*/\1/p' output/table_benchmark.json)
    echo "${pass},${max_ms},${avg_ms}" >> output/table_passes.csv
done

# final pass leaves the canonical artifacts in place
demo/property_table/bin/property_table

# aggregate the upper bound across all passes
awk -F, 'NR>1 {if ($2+0 > max) max=$2+0; sum+=$3; n++}
         END {printf "{\n  \"passes\": %d,\n  \"worst_max_table_ms\": %.4f,\n  \"mean_avg_table_ms\": %.4f\n}\n", n, max, sum/n}' \
    output/table_passes.csv > output/table_passes_summary.json

echo
echo "== geometry =="
cat output/table_metrics.json
echo "== scroll =="
cat output/table_scroll.json
echo "== benchmark =="
cat output/table_benchmark.json
