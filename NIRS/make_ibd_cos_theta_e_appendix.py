#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Publication-quality appendix plot: IBD positron angular distribution vs cos(theta_e+).

Style follows python_analisys/main.ipynb (angular_dNdcos.png, positron panel) with
NIRS plot conventions (dpi=300, single-panel for presentation appendix).

Inputs (first existing):
  - sim/run_nirs/ibd_kinematics.txt
  - sim/run_npe_sum/ibd_kinematics.txt
  - NIRS/data/ibd_cos_theta_e_hist.dat

Output:
  - NIRS/plots/ibd_cos_theta_e_appendix.png
"""

from __future__ import annotations

from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt


def _load_cos_theta(repo: Path) -> np.ndarray:
    kin_candidates = [
        repo / "sim" / "run_nirs" / "ibd_kinematics.txt",
        repo / "sim" / "run_npe_sum" / "ibd_kinematics.txt",
        repo / "sim" / "build" / "ibd_kinematics.txt",
    ]
    for path in kin_candidates:
        if not path.exists() or path.stat().st_size == 0:
            continue
        values = []
        with path.open("r", encoding="utf-8", errors="replace") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split()
                if len(parts) < 5:
                    continue
                try:
                    values.append(float(parts[4]))
                except ValueError:
                    continue
        if values:
            print(f"[read] {path} ({len(values)} events)")
            return np.asarray(values, dtype=float)

    dat_path = repo / "NIRS" / "data" / "ibd_cos_theta_e_hist.dat"
    if dat_path.exists():
        data = np.loadtxt(dat_path, comments="#")
        if len(data) > 0:
            width = (data[1, 0] - data[0, 0]) if len(data) > 1 else 0.04
            expanded = np.repeat(data[:, 0], data[:, 1].astype(int))
            print(f"[read] {dat_path} (expanded {len(expanded)} events from histogram)")
            return expanded

    raise FileNotFoundError("No IBD positron angular data found")


def main() -> None:
    repo = Path(__file__).resolve().parents[1]
    plots_dir = repo / "NIRS" / "plots"
    plots_dir.mkdir(parents=True, exist_ok=True)

    cos_theta = _load_cos_theta(repo)
    mean_cos = float(np.mean(cos_theta))
    n_events = len(cos_theta)

    cos_bins = np.linspace(-1.0, 1.0, 51)

    fig, ax = plt.subplots(figsize=(8, 5.5))
    ax.hist(
        cos_theta,
        bins=cos_bins,
        color="#1f77b4",
        edgecolor="black",
        linewidth=0.35,
        alpha=0.92,
    )
    ax.axvline(
        mean_cos,
        color="red",
        linestyle="--",
        linewidth=1.6,
        label=rf"$\langle\cos\theta_{{e^+}}\rangle = {mean_cos:.3f}$",
    )
    ax.axvline(0.0, color="gray", linestyle=":", linewidth=1.0, alpha=0.7)

    ax.set_xlabel(
        r"$\cos\theta_{e^+}$  (угол между импульсом $e^+$ и $\hat{\nu}_e$)",
        fontsize=18,
    )
    ax.set_ylabel(r"$dN/d\cos\theta_{e^+}$ (событий / бин)", fontsize=18)
    ax.set_title(
        "Угловое распределение позитрона в IBD\n"
        rf"Vogel--Beacom, $N={n_events/1e6:.1f}\times10^6$; "
        r"$\cos\theta_{e^+}=+1$ — вдоль $\hat{\nu}_e$",
        fontsize=19,
    )
    ax.set_xlim(-1.0, 1.0)
    ax.tick_params(axis="both", labelsize=15)
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper right", framealpha=0.95, fontsize=16)

    fig.tight_layout()
    out = plots_dir / "ibd_cos_theta_e_appendix.png"
    fig.savefig(out, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"[write] {out}")
    print(f"[stats] <cos theta_e+> = {mean_cos:.4f}")


if __name__ == "__main__":
    main()
