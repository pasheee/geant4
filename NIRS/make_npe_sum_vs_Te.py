#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Build a histogram of aggregated photoelectrons sum(Npe) versus positron kinetic
energy Te+ directly from a per-event IBD run (not by convolving figures 3 and 4).

Inputs (from sim/run_npe_sum/):
  - ibd_positron_spectrum.txt : # event_id  Enu_MeV  Te+_MeV
  - pe_asymmetry.txt          : # event_id  Npe_top  Npe_bottom  Npe_total  asym  N_fired_PMTs

Outputs:
  - NIRS/data/npe_sum_vs_Te_trig3.dat : # Te_center_MeV  sum_Npe  count
  - NIRS/plots/npe_sum_vs_Te.png
"""

from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt


def main():
    repo = Path(__file__).resolve().parents[1]
    run_dir = repo / "sim" / "run_npe_sum"
    data_dir = repo / "NIRS" / "data"
    plots_dir = repo / "NIRS" / "plots"
    data_dir.mkdir(parents=True, exist_ok=True)
    plots_dir.mkdir(parents=True, exist_ok=True)

    spec_path = run_dir / "ibd_positron_spectrum.txt"
    pe_path = run_dir / "pe_asymmetry.txt"

    for p in (spec_path, pe_path):
        if not p.exists() or p.stat().st_size == 0:
            print(f"[skip] Missing or empty input file: {p}")
            return

    # event_id -> Te+ (MeV)
    te_by_event = {}
    with open(spec_path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 3:
                continue
            try:
                eid = int(float(parts[0]))
                te = float(parts[2])
            except ValueError:
                continue
            te_by_event[eid] = te

    if not te_by_event:
        print(f"[skip] No positron spectrum entries parsed from {spec_path}")
        return

    # Binning: Te+ in [0, 8] MeV, 80 bins (0.1 MeV)
    n_bins = 80
    lo, hi = 0.0, 8.0
    edges = np.linspace(lo, hi, n_bins + 1)
    centers = 0.5 * (edges[:-1] + edges[1:])
    width = (hi - lo) / n_bins

    sum_npe = np.zeros(n_bins)
    count = np.zeros(n_bins, dtype=np.int64)

    n_total = 0
    n_trig = 0
    with open(pe_path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 6:
                continue
            try:
                eid = int(float(parts[0]))
                npe_total = float(parts[3])
                n_fired = int(float(parts[5]))
            except ValueError:
                continue
            n_total += 1
            if n_fired < 3:
                continue
            te = te_by_event.get(eid)
            if te is None:
                continue
            if te < lo or te >= hi:
                continue
            b = int((te - lo) / width)
            if b < 0 or b >= n_bins:
                continue
            sum_npe[b] += npe_total
            count[b] += 1
            n_trig += 1

    # Write the .dat file
    dat_path = data_dir / "npe_sum_vs_Te_trig3.dat"
    with open(dat_path, "w") as f:
        f.write("# Te_center_MeV  sum_Npe  count\n")
        for c, s, n in zip(centers, sum_npe, count):
            f.write(f"{c:.4f}  {s:.6g}  {int(n)}\n")
    print(f"[write] {dat_path}")

    # Plot
    fig, ax = plt.subplots(figsize=(12, 5))
    ax.bar(centers, sum_npe, width=width, color="blue", alpha=0.7)
    ax.set_xlabel(r"Positron Kinetic Energy $T_{e^+}$ (MeV)", fontsize=18)
    ax.set_ylabel(r"$N_{pe}$", fontsize=18)
    ax.set_title("Photoelectrons vs positron energy (N_fired >= 3)", fontsize=19)
    ax.tick_params(axis='both', labelsize=15)
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    png_path = plots_dir / "npe_sum_vs_Te.png"
    fig.savefig(png_path, dpi=300)
    plt.close(fig)
    print(f"[write] {png_path}")

    total_sum = float(sum_npe.sum())
    total_count = int(count.sum())
    mean_npe = (total_sum / total_count) if total_count > 0 else float("nan")
    peak_bin = int(np.argmax(sum_npe))
    print(f"[stats] triggered events binned: {total_count} (of {n_total} total)")
    print(f"[stats] peak Te+ bin center: {centers[peak_bin]:.3f} MeV "
          f"(sum_Npe={sum_npe[peak_bin]:.6g})")
    print(f"[stats] total sum(Npe): {total_sum:.6g}")
    print(f"[stats] mean Npe per triggered event: {mean_npe:.4f}")


if __name__ == "__main__":
    main()
