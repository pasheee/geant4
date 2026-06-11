#!/bin/bash
# Prompt positron asymmetry vs incoming antineutrino direction.
# Runs IBD with Vogel-Beacom angular model, no neutron (prompt-only).
set -eo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO"

if command -v conda >/dev/null 2>&1; then
    eval "$(conda shell.bash hook)"
    conda activate g4env
fi

N_EVENTS="${N_EVENTS:-150000}"
SEED="${SIM_SEED:-42}"
TEMPLATE="$REPO/sim/macros/asym_nu_template.mac"
OUT_ROOT="$REPO/sim/run_asym_dir"
mkdir -p "$OUT_ROOT"

run_one() {
    local tag="$1"
    local nu_dir="$2"
    local outdir="$OUT_ROOT/$tag"
    mkdir -p "$outdir"
    sed -e "s|__NU_DIR__|${nu_dir}|g" \
        -e "s|__N_EVENTS__|${N_EVENTS}|g" \
        "$TEMPLATE" > "$outdir/run.mac"
    echo "=== $tag: nu_dir=$nu_dir, N=$N_EVENTS, seed=$SEED ==="
    (cd "$outdir" && SIM_SEED="$SEED" "$REPO/sim/build/sim" run.mac > log.txt 2>&1)
    echo "  done: $outdir/pe_asymmetry.txt"
}

run_one "nu_minus_z" "0 0 -1" &
PID1=$!
run_one "nu_plus_z"  "0 0  1" &
PID2=$!
wait $PID1 $PID2

echo "All asymmetry direction runs complete in $OUT_ROOT"
