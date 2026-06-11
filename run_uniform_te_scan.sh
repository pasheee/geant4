#!/bin/bash
# Uniform mono-energetic positron scan for <Npe>(Te+) with equal statistics per point.
#
# Grid: Te = 0.0, 0.1, ..., 8.0 MeV (81 points).
# Output: sim/run_uniform_te/Te_<energy>/pe_asymmetry.txt
#
# Usage:
#   ./run_uniform_te_scan.sh
#   EVENTS_PER_POINT=10000 MAX_PARALLEL=4 ./run_uniform_te_scan.sh

set -eo pipefail

source /opt/homebrew/Caskroom/miniforge/base/etc/profile.d/conda.sh
conda activate g4env

REPO="$(cd "$(dirname "$0")" && pwd)"
SIM="$REPO/sim/build/sim"
TEMPLATE="$REPO/sim/macros/pos_uniform_template.mac"
OUT_ROOT="$REPO/sim/run_uniform_te"
EVENTS_PER_POINT="${EVENTS_PER_POINT:-20000}"
MAX_PARALLEL="${MAX_PARALLEL:-8}"

if [[ ! -x "$SIM" ]]; then
    echo "Building sim..."
    (cd "$REPO/sim/build" && cmake .. && make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)")
fi

mkdir -p "$OUT_ROOT"

run_point() {
    local te="$1"
    local tag="${te//./p}"
    local rundir="$OUT_ROOT/Te_${tag}"
    mkdir -p "$rundir"
    local macro="$rundir/run.mac"
    local pe_out="$rundir/pe_asymmetry.txt"

    if [[ -f "$pe_out" ]]; then
        local nlines
        nlines=$(wc -l < "$pe_out" | tr -d ' ')
        if [[ "$nlines" -ge $((EVENTS_PER_POINT - 100)) ]]; then
            echo "Skip Te=${te} MeV (${nlines} events already)"
            return 0
        fi
    fi

    sed "s/__ENERGY__/${te}/g; s/__EVENTS__/${EVENTS_PER_POINT}/g" "$TEMPLATE" > "$macro"
    echo "Running Te=${te} MeV (${EVENTS_PER_POINT} events)..."
    (
        cd "$rundir"
        local seed
        seed=$(python3 -c "print(int(float('${te}')*1000) + 42)")
        SIM_SEED="$seed" "$SIM" "$macro" > log.txt 2>&1
    )
    echo "Done Te=${te} MeV"
}

export REPO SIM TEMPLATE OUT_ROOT EVENTS_PER_POINT
export -f run_point

energies=()
for i in $(seq 0 80); do
    energies+=("$(awk -v i="$i" 'BEGIN{printf "%.1f", i/10}')")
done

echo "Uniform Te scan: ${#energies[@]} points, ${EVENTS_PER_POINT} events/point, max ${MAX_PARALLEL} parallel"

pids=()
for te in "${energies[@]}"; do
    run_point "$te" &
    pids+=($!)
    if [[ ${#pids[@]} -ge $MAX_PARALLEL ]]; then
        wait "${pids[0]}"
        pids=("${pids[@]:1}")
    fi
done
for pid in "${pids[@]}"; do
    wait "$pid"
done

echo "Uniform Te scan complete. Processing plot data..."
python "$REPO/NIRS/make_plots_data.py" --skip-cherenkov

echo "ALL DONE (${#energies[@]} points x ${EVENTS_PER_POINT} events)"
