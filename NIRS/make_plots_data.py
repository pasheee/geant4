#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generate small histogram tables for LaTeX (pgfplots) used in NIRS/main.tex.

Inputs (produced by the Geant4 simulation, usually in sim/build/):
  - ibd_positron_spectrum.txt     (# event_id Enu_MeV Te+_MeV)
  - ibd_kinematics.txt            (# event_id Enu_MeV Ee_MeV Te_MeV cosThetaE ...)
  - pe_asymmetry.txt              (# event_id Npe_top Npe_bottom Npe_total asym)
  - chernkov_spectrum.txt         (photon energy in eV, one per line)

Outputs (written to NIRS/data/):
  - positron_Te_hist.dat
  - asym_hist.dat
  - cherenkov_lambda_hist.dat
  - npe_vs_Te_mean.dat            (<Npe> vs positron kinetic energy)
  - ibd_cos_theta_e_hist.dat
  - ibd_cos_theta_n_hist.dat
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

    # 1b) IBD angular distributions relative to the incoming antineutrino.
    kin_path = sim_build / "ibd_kinematics.txt"
    cos_theta_e = []
    cos_theta_n = []
    if kin_path.exists():
        for p in _read_columns(kin_path):
            if len(p) >= 11:
                try:
                    cos_theta_e.append(float(p[4]))
                    cos_theta_n.append(float(p[10]))
                except ValueError:
                    pass

    for filename, values, header in [
        ("ibd_cos_theta_e_hist.dat", cos_theta_e, "# cos_theta_e_center  count\n"),
        ("ibd_cos_theta_n_hist.dat", cos_theta_n, "# cos_theta_n_center  count\n"),
    ]:
        centers, counts = make_hist(values, vmin=-1.0, vmax=1.0, nbins=50)
        out = out_dir / filename
        with out.open("w") as f:
            f.write(header)
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

    # 4b) Neutron-capture prompt gamma spectrum (per dopant).
    # Inputs (written by RunAction when /analysis/writeCaptureGammas is on):
    #   capture_gammas_<Dop>.txt   : "Zres Ares Egamma_MeV"  (one prompt gamma per line)
    #   capture_summary_<Dop>.txt  : "Zres Ares nGamma sumEgamma nConvE sumEconvE"
    # We isolate captures on the dopant nucleus (Z=48 Cd, Z=64 Gd) so the cascade is
    # not diluted by the dominant 2.2 MeV hydrogen-capture channel.
    Z_OF_DOPANT = {"Cd": 48, "Gd": 64}
    capture_stats = []
    for dopant in ["Cd", "Gd"]:
        zdop = Z_OF_DOPANT[dopant]
        gpath = sim_build / f"capture_gammas_{dopant}.txt"
        spath = sim_build / f"capture_summary_{dopant}.txt"
        if not gpath.exists():
            continue

        # Per-gamma spectrum for the dopant captures only.
        egammas = []
        for p in _read_columns(gpath):
            if len(p) >= 3:
                try:
                    if int(p[0]) == zdop:
                        egammas.append(float(p[2]))
                except ValueError:
                    pass
        if not egammas:
            continue
        c, h = make_hist(egammas, vmin=0.0, vmax=9.5, nbins=95)
        ntot = sum(h)
        out_hist = out_dir / f"capture_gamma_spectrum_{dopant}.dat"
        with out_hist.open("w") as f:
            f.write("# Egamma_center_MeV  count  fraction_per_bin\n")
            for x, y in zip(c, h):
                f.write(f"{x:.4f} {y} {(y / ntot if ntot else 0.0):.6e}\n")
        print(" -", out_hist.as_posix())

        # Per-capture cascade summary (mean total energy, multiplicity, conv-e share).
        n_cap = 0
        sum_eg = 0.0
        sum_mult = 0
        n_with_conv = 0
        sum_conv = 0.0
        if spath.exists():
            for p in _read_columns(spath):
                if len(p) >= 6:
                    try:
                        if int(p[0]) != zdop:
                            continue
                        n_cap += 1
                        sum_mult += int(p[2])
                        sum_eg += float(p[3])
                        nconv = int(p[4])
                        if nconv > 0:
                            n_with_conv += 1
                        sum_conv += float(p[5])
                    except ValueError:
                        pass
        if n_cap > 0:
            capture_stats.append((
                dopant, zdop, n_cap,
                sum_eg / n_cap,            # <sum E_gamma> per capture (MeV)
                sum_mult / n_cap,          # <gamma multiplicity>
                sum(egammas) / len(egammas),  # <single gamma energy> (MeV)
                100.0 * n_with_conv / n_cap,  # fraction of captures with conv e- (%)
                sum_conv / n_cap,          # <conv-e energy> per capture (MeV)
            ))

    if capture_stats:
        out_stats = out_dir / "capture_gamma_stats.dat"
        with out_stats.open("w") as f:
            f.write("# dopant Z Ncaptures meanSumEgamma_MeV meanMultiplicity "
                    "meanSingleGamma_MeV fracWithConvE_percent meanConvE_MeV\n")
            for row in capture_stats:
                f.write("{0} {1} {2} {3:.4f} {4:.4f} {5:.4f} {6:.4f} {7:.6f}\n".format(*row))
        print(" -", out_stats.as_posix())

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
    # Adaptive binning: merge neighbours until merged bin has >= MIN_MERGE counts.
    # Fine bins stay as-is in the well-populated region; sparse tail bins are grouped.
    MIN_MERGE = 30
    with out.open("w") as f:
        f.write("# Te_center_MeV  mean_Npe  stderr_Npe  count (N_fired >= 3)\n")
        ib = 0
        while ib < nbins_prof:
            if cnt_t[ib] == 0:
                ib += 1
                continue
            if cnt_t[ib] >= MIN_MERGE:
                # Good bin — write directly
                center = Tmin_prof + (ib + 0.5) * width_prof
                mean   = s1_t[ib] / cnt_t[ib]
                mean2  = s2_t[ib] / cnt_t[ib]
                var    = max(0.0, mean2 - mean * mean)
                stderr = math.sqrt(var / cnt_t[ib])
                f.write(f"{center:.5f} {mean:.6f} {stderr:.6f} {cnt_t[ib]}\n")
                ib += 1
            else:
                # Sparse bin — merge forward until we accumulate enough counts
                m_cnt, m_s1, m_s2 = 0, 0.0, 0.0
                start = ib
                while ib < nbins_prof and m_cnt < MIN_MERGE:
                    m_cnt += cnt_t[ib]
                    m_s1  += s1_t[ib]
                    m_s2  += s2_t[ib]
                    ib += 1
                if m_cnt >= MIN_MERGE // 2:
                    center = Tmin_prof + (start + (ib - start) / 2.0) * width_prof
                    mean   = m_s1 / m_cnt
                    mean2  = m_s2 / m_cnt
                    var    = max(0.0, mean2 - mean * mean)
                    stderr = math.sqrt(var / m_cnt)
                    f.write(f"{center:.5f} {mean:.6f} {stderr:.6f} {m_cnt}\n")
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

    # D) Uniform mono-energetic Te scan (N_fired >= 3)
    uniform_root = repo / "sim" / "run_uniform_te"
    if uniform_root.exists():
        rows = []
        for te_dir in sorted(uniform_root.iterdir()):
            if not te_dir.is_dir() or not te_dir.name.startswith("Te_"):
                continue
            te_str = te_dir.name[3:].replace("p", ".")
            try:
                te_val = float(te_str)
            except ValueError:
                continue
            pe_dp = te_dir / "pe_asymmetry.txt"
            if not pe_dp.exists():
                continue
            npes_t = []
            for p in _read_columns(pe_dp):
                if len(p) >= 6:
                    try:
                        nfired = int(float(p[5]))
                        npe = float(p[3])
                    except ValueError:
                        continue
                    if nfired >= TRIG_THR:
                        npes_t.append(npe)
            if not npes_t:
                continue
            n = len(npes_t)
            mean = sum(npes_t) / n
            mean2 = sum(x * x for x in npes_t) / n
            var = max(0.0, mean2 - mean * mean)
            stderr = math.sqrt(var / n)
            rows.append((te_val, mean, stderr, n))
        if rows:
            rows.sort(key=lambda r: r[0])
            out = out_dir / "npe_vs_Te_mean_uniform_trig3.dat"
            with out.open("w") as f:
                f.write("# Te_MeV  mean_Npe  stderr_Npe  count (N_fired >= 3, mono-energetic uniform grid)\n")
                for te_val, mean, stderr, n in rows:
                    f.write(f"{te_val:.1f} {mean:.6f} {stderr:.6f} {n}\n")
            print(" -", out.as_posix())

    # E) Directional prompt asymmetry (nu along +/- z, prompt-only IBD)
    asym_root = repo / "sim" / "run_asym_dir"
    asym_dirs = {
        "minus_z": asym_root / "nu_minus_z",
        "plus_z": asym_root / "nu_plus_z",
    }
    mean_rows = []
    for tag, run_dir in asym_dirs.items():
        pe_dp = run_dir / "pe_asymmetry.txt"
        if not pe_dp.exists():
            continue
        asym_vals = []
        for p in _read_columns(pe_dp):
            if len(p) >= 6:
                try:
                    nfired = int(float(p[5]))
                    a = float(p[4])
                except ValueError:
                    continue
                if nfired >= TRIG_THR:
                    asym_vals.append(a)
        if not asym_vals:
            continue
        centers, counts = make_hist(asym_vals, vmin=-1.0, vmax=1.0, nbins=40)
        out_h = out_dir / f"asym_hist_nu_{tag}_trig3.dat"
        with out_h.open("w") as f:
            f.write(f"# asym_center  count (nu_{tag}, N_fired >= {TRIG_THR}, prompt-only IBD)\n")
            for x, y in zip(centers, counts):
                f.write(f"{x:.5f} {y}\n")
        print(" -", out_h.as_posix())
        n = len(asym_vals)
        mean = sum(asym_vals) / n
        mean2 = sum(x * x for x in asym_vals) / n
        var = max(0.0, mean2 - mean * mean)
        stderr = math.sqrt(var / n)
        mean_rows.append((tag, mean, stderr, n))

        # <A>(T_e+) profile joined with ibd_positron_spectrum.txt
        spec_dp = run_dir / "ibd_positron_spectrum.txt"
        Te_by_event = {}
        if spec_dp.exists():
            for p in _read_columns(spec_dp):
                if len(p) >= 3:
                    try:
                        Te_by_event[int(float(p[0]))] = float(p[2])
                    except ValueError:
                        pass
        nbins_a = 16
        Tmin_a, Tmax_a = 2.0, 8.0
        width_a = (Tmax_a - Tmin_a) / nbins_a
        cnt_a = [0] * nbins_a
        s1_a = [0.0] * nbins_a
        s2_a = [0.0] * nbins_a
        for p in _read_columns(pe_dp):
            if len(p) >= 6:
                try:
                    eid = int(float(p[0]))
                    nfired = int(float(p[5]))
                    a = float(p[4])
                except ValueError:
                    continue
                if nfired < TRIG_THR:
                    continue
                Te_i = Te_by_event.get(eid)
                if Te_i is None or Te_i < Tmin_a or Te_i >= Tmax_a:
                    continue
                ib = int((Te_i - Tmin_a) / (Tmax_a - Tmin_a) * nbins_a)
                if 0 <= ib < nbins_a:
                    cnt_a[ib] += 1
                    s1_a[ib] += a
                    s2_a[ib] += a * a
        out_prof = out_dir / f"asym_vs_Te_nu_{tag}_trig3.dat"
        with out_prof.open("w") as f:
            f.write(f"# Te_center_MeV  mean_A  stderr_A  count (nu_{tag}, prompt-only)\n")
            for ib in range(nbins_a):
                center = Tmin_a + (ib + 0.5) * width_a
                if cnt_a[ib] <= 0:
                    f.write(f"{center:.5f} 0 0 0\n")
                    continue
                m = s1_a[ib] / cnt_a[ib]
                m2 = s2_a[ib] / cnt_a[ib]
                v = max(0.0, m2 - m * m)
                se = math.sqrt(v / cnt_a[ib])
                f.write(f"{center:.5f} {m:.6f} {se:.6f} {cnt_a[ib]}\n")
        print(" -", out_prof.as_posix())

    if mean_rows:
        out_m = out_dir / "asym_mean_nu_dir_trig3.dat"
        with out_m.open("w") as f:
            f.write("# nu_direction_tag  mean_A  stderr_A  count (N_fired >= 3)\n")
            for tag, mean, stderr, n in mean_rows:
                f.write(f"{tag} {mean:.6f} {stderr:.6f} {n}\n")
        print(" -", out_m.as_posix())

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


