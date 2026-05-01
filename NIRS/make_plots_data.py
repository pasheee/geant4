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

    nbins_prof = 90
    Tmin_prof, Tmax_prof = 0.0, 9.0
    width_prof = (Tmax_prof - Tmin_prof) / nbins_prof
    cnt = [0] * nbins_prof
    s1 = [0.0] * nbins_prof
    s2 = [0.0] * nbins_prof
    
    s1_fired = [0.0] * nbins_prof
    s2_fired = [0.0] * nbins_prof

    npe_2_3 = []
    npe_5_6 = []
    npe_8_9 = []

    if pe_path.exists() and Te_by_event:
        for p in _read_columns(pe_path):
            if len(p) >= 4:
                try:
                    eid = int(float(p[0]))
                    npe = float(p[3])
                except ValueError:
                    continue
                
                nfired = 0.0
                if len(p) >= 6:
                    try:
                        nfired = float(p[5])
                    except ValueError:
                        pass

                Te_i = Te_by_event.get(eid)
                if Te_i is None:
                    continue
                
                if 2.0 <= Te_i < 3.0:
                    npe_2_3.append(npe)
                elif 5.0 <= Te_i < 6.0:
                    npe_5_6.append(npe)
                elif 8.0 <= Te_i < 9.0:
                    npe_8_9.append(npe)

                if Te_i < Tmin_prof or Te_i >= Tmax_prof:
                    continue
                ib = int((Te_i - Tmin_prof) / (Tmax_prof - Tmin_prof) * nbins_prof)
                if 0 <= ib < nbins_prof:
                    cnt[ib] += 1
                    s1[ib] += npe
                    s2[ib] += npe * npe
                    s1_fired[ib] += nfired
                    s2_fired[ib] += nfired * nfired

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

    out_fired = out_dir / "n_fired_vs_Te_mean.dat"
    with out_fired.open("w") as f:
        f.write("# Te_center_MeV  mean_N_fired  stderr_N_fired  count\n")
        for ib in range(nbins_prof):
            center = Tmin_prof + (ib + 0.5) * width_prof
            if cnt[ib] <= 0:
                f.write(f"{center:.5f} 0 0 0\n")
                continue
            mean = s1_fired[ib] / cnt[ib]
            mean2 = s2_fired[ib] / cnt[ib]
            var = max(0.0, mean2 - mean * mean)
            stderr = math.sqrt(var / cnt[ib]) if cnt[ib] > 0 else 0.0
            f.write(f"{center:.5f} {mean:.6f} {stderr:.6f} {cnt[ib]}\n")

    for slice_name, data_arr in [("2_3", npe_2_3), ("5_6", npe_5_6), ("8_9", npe_8_9)]:
        if not data_arr:
            continue
        c, h = make_hist(data_arr, vmin=0, vmax=2500, nbins=50)
        out_hist = out_dir / f"npe_hist_Te_{slice_name}.dat"
        with out_hist.open("w") as f:
            f.write("# Npe_center  count\n")
            for x, y in zip(c, h):
                f.write(f"{x:.1f} {y}\n")

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

    # 4) Neutron lifetime histograms
    for dopant in ["Cd", "Gd"]:
        lifetime_path = sim_build / f"neutron_lifetime_{dopant}.txt"
        if lifetime_path.exists():
            times = []
            for p in _read_columns(lifetime_path):
                if len(p) >= 1:
                    try:
                        times.append(float(p[0]) * 1e-3) # ns to us
                    except ValueError:
                        pass
            
            if times:
                c, h = make_hist(times, vmin=0, vmax=200, nbins=100) # up to 200 us
                out_hist = out_dir / f"lifetime_hist_{dopant}.dat"
                with out_hist.open("w") as f:
                    f.write("# lifetime_us_center count\n")
                    for x, y in zip(c, h):
                        f.write(f"{x:.3f} {y}\n")
                print(" -", out_hist.as_posix())

    # 5) Neutron capture Npe histograms (linear hist + log-friendly hist + survival curve)
    import bisect
    for dopant in ["Cd", "Gd"]:
        pe_dopant_path = sim_build / f"pe_{dopant.lower()}.txt"
        if pe_dopant_path.exists():
            npes = []
            for p in _read_columns(pe_dopant_path):
                # event_id Npe_top Npe_bottom Npe_total asym N_fired_PMTs
                if len(p) >= 4:
                    try:
                        npes.append(float(p[3]))
                    except ValueError:
                        pass

            if npes:
                # 5a) Linear histogram in [0, 200] (legacy, full bins including zeros)
                c, h = make_hist(npes, vmin=0, vmax=200, nbins=50)
                out_hist = out_dir / f"npe_hist_{dopant}.dat"
                with out_hist.open("w") as f:
                    f.write("# Npe_center count\n")
                    for x, y in zip(c, h):
                        f.write(f"{x:.1f} {y}\n")
                print(" -", out_hist.as_posix())

                # 5b) Wider histogram in [0, 300] for log-Y step plot (skip zero bins
                #     so that log scale does not blow up; preserves the visible tail).
                cw, hw = make_hist(npes, vmin=0, vmax=300, nbins=60)
                out_hist_log = out_dir / f"npe_hist_log_{dopant}.dat"
                with out_hist_log.open("w") as f:
                    f.write("# Npe_center count (zero bins skipped)\n")
                    for x, y in zip(cw, hw):
                        if y > 0:
                            f.write(f"{x:.1f} {y}\n")
                print(" -", out_hist_log.as_posix())

                # 5c) Survival (right-tail) curve P(N_pe >= x).
                #     Built directly from the unbinned distribution -> exact, monotone.
                npes_sorted = sorted(int(round(v)) for v in npes)
                N = len(npes_sorted)
                xmax_surv = 500
                out_surv = out_dir / f"npe_survival_{dopant}.dat"
                with out_surv.open("w") as f:
                    f.write("# Npe_threshold  P(Npe>=x)\n")
                    for x in range(0, xmax_surv + 1):
                        idx = bisect.bisect_left(npes_sorted, x)
                        p_ge = (N - idx) / N
                        f.write(f"{x} {p_ge:.6f}\n")
                print(" -", out_surv.as_posix())

    # 6) N_fired survival curves (for trigger efficiency analysis)
    # IBD prompt: pe_asymmetry.txt col 5
    # Delayed Cd/Gd: pe_cd.txt / pe_gd.txt col 5
    fired_sources = [
        ("ibd", pe_path, 5),
        ("Cd",  sim_build / "pe_cd.txt", 5),
        ("Gd",  sim_build / "pe_gd.txt", 5),
    ]
    for tag, src_path, col in fired_sources:
        if not src_path.exists():
            continue
        fired = []
        for p in _read_columns(src_path):
            if len(p) > col:
                try:
                    fired.append(int(float(p[col])))
                except ValueError:
                    pass
        if not fired:
            continue
        fired_sorted = sorted(fired)
        N_f = len(fired_sorted)
        out_surv = out_dir / f"nfired_survival_{tag}.dat"
        with out_surv.open("w") as f:
            f.write("# Nfired_threshold  P(Nfired>=x)\n")
            for x in range(0, 39):
                idx = bisect.bisect_left(fired_sorted, x)
                f.write(f"{x} {(N_f - idx) / N_f:.6f}\n")
        print(" -", out_surv.as_posix())

    # 7) Optical photons generated histograms for positron percentiles (p10, p50, p90)
    for tag in ["p10", "p50", "p90"]:
        photons_path = sim_build / f"photons_{tag}.txt"
        if photons_path.exists():
            photons = []
            for p in _read_columns(photons_path):
                if len(p) >= 2:
                    try:
                        photons.append(int(p[1]))
                    except ValueError:
                        pass
            
            if photons:
                c, h = make_hist(photons, vmin=0, vmax=max(photons)*1.1, nbins=60)
                out_hist = out_dir / f"photons_hist_{tag}.dat"
                with out_hist.open("w") as f:
                    f.write("# generated_photons_center count\n")
                    for x, y in zip(c, h):
                        f.write(f"{x:.1f} {y}\n")
                print(" -", out_hist.as_posix())

    # ========= TRIGGER-FILTERED DATA (N_fired >= 3) =========
    TRIG_THR = 3

    # Collect N_fired per event from IBD run
    fired_ibd_map = {}
    if pe_path.exists():
        for p in _read_columns(pe_path):
            if len(p) >= 6:
                try:
                    fired_ibd_map[int(float(p[0]))] = int(float(p[5]))
                except ValueError:
                    pass

    # A) Positron Te spectrum (trig3)
    Te_trig = []
    if ibd_path.exists():
        for p in _read_columns(ibd_path):
            if len(p) >= 3:
                try:
                    eid = int(float(p[0]))
                    if fired_ibd_map.get(eid, 0) >= TRIG_THR:
                        Te_trig.append(float(p[2]))
                except ValueError:
                    pass
    centers, counts = make_hist(Te_trig, vmin=0.0, vmax=8.0, nbins=80)
    out = out_dir / "positron_Te_hist_trig3.dat"
    with out.open("w") as f:
        f.write("# Te_center_MeV  count (N_fired >= 3)\n")
        for x, y in zip(centers, counts):
            f.write(f"{x:.5f} {y}\n")
    print(" -", out.as_posix())

    # B) <Npe> vs T_e+ profile (trig3)
    cnt_t = [0] * nbins_prof
    s1_t  = [0.0] * nbins_prof
    s2_t  = [0.0] * nbins_prof
    if pe_path.exists() and Te_by_event:
        for p in _read_columns(pe_path):
            if len(p) >= 6:
                try:
                    eid    = int(float(p[0]))
                    npe    = float(p[3])
                    nfired = int(float(p[5]))
                except ValueError:
                    continue
                if nfired < TRIG_THR:
                    continue
                Te_i = Te_by_event.get(eid)
                if Te_i is None or Te_i < Tmin_prof or Te_i >= Tmax_prof:
                    continue
                ib = int((Te_i - Tmin_prof) / (Tmax_prof - Tmin_prof) * nbins_prof)
                if 0 <= ib < nbins_prof:
                    cnt_t[ib] += 1
                    s1_t[ib]  += npe
                    s2_t[ib]  += npe * npe
    out = out_dir / "npe_vs_Te_mean_trig3.dat"
    with out.open("w") as f:
        f.write("# Te_center_MeV  mean_Npe  stderr_Npe  count (N_fired >= 3)\n")
        for ib in range(nbins_prof):
            if cnt_t[ib] <= 0:
                continue
            center = Tmin_prof + (ib + 0.5) * width_prof
            mean   = s1_t[ib] / cnt_t[ib]
            mean2  = s2_t[ib] / cnt_t[ib]
            var    = max(0.0, mean2 - mean * mean)
            stderr = math.sqrt(var / cnt_t[ib])
            f.write(f"{center:.5f} {mean:.6f} {stderr:.6f} {cnt_t[ib]}\n")
    print(" -", out.as_posix())

    # C) Lifetime + Npe histograms for Cd and Gd (trig3)
    for dopant in ["Cd", "Gd"]:
        pe_dp   = sim_build / f"pe_{dopant.lower()}.txt"
        lt_path = sim_build / f"neutron_lifetime_{dopant}.txt"
        if not pe_dp.exists() or not lt_path.exists():
            continue
        pe_rows = list(_read_columns(pe_dp))
        lt_rows = list(_read_columns(lt_path))
        if len(pe_rows) != len(lt_rows):
            print(f"Warning: {dopant} pe/lifetime length mismatch, skipping trig3")
            continue
        npes_t, lts_t = [], []
        for pe_r, lt_r in zip(pe_rows, lt_rows):
            if len(pe_r) < 6 or len(lt_r) < 1:
                continue
            try:
                nfired = int(float(pe_r[5]))
                npe    = float(pe_r[3])
                lt_us  = float(lt_r[0]) * 1e-3
            except ValueError:
                continue
            if nfired >= TRIG_THR:
                npes_t.append(npe)
                lts_t.append(lt_us)
        if lts_t:
            c, h = make_hist(lts_t, vmin=0, vmax=200, nbins=100)
            out_h = out_dir / f"lifetime_hist_{dopant}_trig3.dat"
            with out_h.open("w") as f:
                f.write(f"# lifetime_us_center count (N_fired >= {TRIG_THR})\n")
                for x, y in zip(c, h):
                    f.write(f"{x:.3f} {y}\n")
            print(" -", out_h.as_posix())
        if npes_t:
            cw, hw = make_hist(npes_t, vmin=0, vmax=300, nbins=60)
            out_h = out_dir / f"npe_hist_log_{dopant}_trig3.dat"
            with out_h.open("w") as f:
                f.write(f"# Npe_center count (N_fired >= {TRIG_THR})\n")
                for x, y in zip(cw, hw):
                    if y > 0:
                        f.write(f"{x:.1f} {y}\n")
            print(" -", out_h.as_posix())
            npes_sorted_t = sorted(int(round(v)) for v in npes_t)
            N_t = len(npes_sorted_t)
            out_h = out_dir / f"npe_survival_{dopant}_trig3.dat"
            with out_h.open("w") as f:
                f.write(f"# Npe_threshold  P(Npe>=x) (N_fired >= {TRIG_THR})\n")
                for x in range(0, 501):
                    idx   = bisect.bisect_left(npes_sorted_t, x)
                    f.write(f"{x} {(N_t - idx) / N_t:.6f}\n")
            print(" -", out_h.as_posix())

    print("Wrote:")
    print(" -", (out_dir / "positron_Te_hist.dat").as_posix())
    print(" -", (out_dir / "asym_hist.dat").as_posix())
    print(" -", (out_dir / "npe_vs_Te_mean.dat").as_posix())
    print(" -", (out_dir / "n_fired_vs_Te_mean.dat").as_posix())
    for slice_name in ["2_3", "5_6", "8_9"]:
        p = out_dir / f"npe_hist_Te_{slice_name}.dat"
        if p.exists():
            print(" -", p.as_posix())
    if (out_dir / "cherenkov_lambda_hist.dat").exists():
        print(" -", (out_dir / "cherenkov_lambda_hist.dat").as_posix())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


