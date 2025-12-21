#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generate small histogram tables for LaTeX (pgfplots) used in NIRS/main.tex.

Inputs (produced by the Geant4 simulation, usually in sim/build/):
  - ibd_positron_spectrum.txt     (# event_id Enu_MeV Te+_MeV)
  - pe_asymmetry.txt              (# event_id Npe_top Npe_bottom Npe_total asym)
  - chernkov_spectrum.txt         (photon energy in eV, one per line)

Outputs (written to NIRS/data/):
  - positron_Te_hist.dat
  - asym_hist.dat
  - cherenkov_lambda_hist.dat
  - npe_vs_Te_mean.dat            (<Npe> vs positron kinetic energy)
"""

from __future__ import annotations

from pathlib import Path
import argparse
import math


def _read_columns(path: Path):
    with path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            yield line.split()


def make_hist(values, vmin, vmax, nbins):
    bins = [0] * nbins
    width = (vmax - vmin) / nbins
    for v in values:
        if v < vmin or v >= vmax:
            continue
        i = int((v - vmin) / (vmax - vmin) * nbins)
        if 0 <= i < nbins:
            bins[i] += 1
    centers = [vmin + (i + 0.5) * width for i in range(nbins)]
    return centers, bins


def _cherenkov_wavelengths_nm(cher_path: Path):
    hc = 1239.841984  # eV*nm
    with cher_path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            try:
                E = float(line)
            except ValueError:
                continue
            if E <= 0:
                continue
            yield hc / E


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--skip-cherenkov", action="store_true",
                    help="Do not (re)generate cherenkov_lambda_hist.dat even if input exists")
    ap.add_argument("--force-cherenkov", action="store_true",
                    help="Force reading chernkov_spectrum.txt even if it is large")
    args = ap.parse_args()

    repo = Path(__file__).resolve().parents[1]
    sim_build = repo / "sim" / "build"
    out_dir = repo / "NIRS" / "data"
    out_dir.mkdir(parents=True, exist_ok=True)

    # 1) Positron kinetic energy spectrum
    ibd_path = sim_build / "ibd_positron_spectrum.txt"
    Te = []
    if ibd_path.exists():
        for p in _read_columns(ibd_path):
            if len(p) >= 3:
                try:
                    Te.append(float(p[2]))
                except ValueError:
                    pass

    centers, counts = make_hist(Te, vmin=0.0, vmax=8.0, nbins=80)
    out = out_dir / "positron_Te_hist.dat"
    with out.open("w") as f:
        f.write("# Te_center_MeV  count\n")
        for x, y in zip(centers, counts):
            f.write(f"{x:.5f} {y}\n")

    # 2) Asymmetry histogram
    pe_path = sim_build / "pe_asymmetry.txt"
    asym = []
    if pe_path.exists():
        for p in _read_columns(pe_path):
            if len(p) >= 5:
                try:
                    asym.append(float(p[4]))
                except ValueError:
                    pass

    centers, counts = make_hist(asym, vmin=-1.0, vmax=1.0, nbins=40)
    out = out_dir / "asym_hist.dat"
    with out.open("w") as f:
        f.write("# asym_center  count\n")
        for x, y in zip(centers, counts):
            f.write(f"{x:.5f} {y}\n")

    # 2b) <Npe> vs Te (mean profile)
    # Join by event_id using the two per-event outputs.
    Te_by_event = {}
    if ibd_path.exists():
        for p in _read_columns(ibd_path):
            if len(p) >= 3:
                try:
                    eid = int(float(p[0]))
                    Te_by_event[eid] = float(p[2])
                except ValueError:
                    pass

    nbins_prof = 80
    Tmin_prof, Tmax_prof = 0.0, 8.0
    width_prof = (Tmax_prof - Tmin_prof) / nbins_prof
    cnt = [0] * nbins_prof
    s1 = [0.0] * nbins_prof
    s2 = [0.0] * nbins_prof
    if pe_path.exists() and Te_by_event:
        for p in _read_columns(pe_path):
            if len(p) >= 4:
                try:
                    eid = int(float(p[0]))
                    npe = float(p[3])
                except ValueError:
                    continue
                Te_i = Te_by_event.get(eid)
                if Te_i is None:
                    continue
                if Te_i < Tmin_prof or Te_i >= Tmax_prof:
                    continue
                ib = int((Te_i - Tmin_prof) / (Tmax_prof - Tmin_prof) * nbins_prof)
                if 0 <= ib < nbins_prof:
                    cnt[ib] += 1
                    s1[ib] += npe
                    s2[ib] += npe * npe

    out = out_dir / "npe_vs_Te_mean.dat"
    with out.open("w") as f:
        f.write("# Te_center_MeV  mean_Npe  stderr_Npe  count\n")
        for ib in range(nbins_prof):
            center = Tmin_prof + (ib + 0.5) * width_prof
            if cnt[ib] <= 0:
                f.write(f"{center:.5f} 0 0 0\n")
                continue
            mean = s1[ib] / cnt[ib]
            mean2 = s2[ib] / cnt[ib]
            var = max(0.0, mean2 - mean * mean)
            # standard error of the mean
            stderr = math.sqrt(var / cnt[ib]) if cnt[ib] > 0 else 0.0
            f.write(f"{center:.5f} {mean:.6f} {stderr:.6f} {cnt[ib]}\n")

    # 3) Cherenkov photon wavelength spectrum
    cher_path = sim_build / "chernkov_spectrum.txt"
    if cher_path.exists() and not args.skip_cherenkov:
        max_size = 200 * 1024 * 1024  # 200 MB
        if (not args.force_cherenkov) and cher_path.stat().st_size > max_size:
            print(f"Skipping Cherenkov histogram: {cher_path} is larger than {max_size} bytes.")
            print("Use --force-cherenkov to process it.")
        else:
            centers, counts = make_hist(_cherenkov_wavelengths_nm(cher_path), vmin=200.0, vmax=650.0, nbins=90)
            out = out_dir / "cherenkov_lambda_hist.dat"
            with out.open("w") as f:
                f.write("# lambda_center_nm  count\n")
                for x, y in zip(centers, counts):
                    f.write(f"{x:.3f} {y}\n")
    elif args.skip_cherenkov:
        print("Skipping Cherenkov histogram (by --skip-cherenkov).")
    else:
        print("Skipping Cherenkov histogram (input file does not exist).")

    print("Wrote:")
    print(" -", (out_dir / "positron_Te_hist.dat").as_posix())
    print(" -", (out_dir / "asym_hist.dat").as_posix())
    print(" -", (out_dir / "npe_vs_Te_mean.dat").as_posix())
    if (out_dir / "cherenkov_lambda_hist.dat").exists():
        print(" -", (out_dir / "cherenkov_lambda_hist.dat").as_posix())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


