#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Script to generate .png plots from the .dat files in NIRS/data.
"""

from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt

def main():
    repo = Path(__file__).resolve().parents[1]
    data_dir = repo / "NIRS" / "data"
    plots_dir = repo / "NIRS" / "plots"
    plots_dir.mkdir(parents=True, exist_ok=True)

    # Helper function to read .dat files
    def load_dat(filename):
        path = data_dir / filename
        if not path.exists():
            return None
        return np.loadtxt(path, comments="#")

    # 1. Positron Te Histogram
    data = load_dat("positron_Te_hist.dat")
    if data is not None and len(data) > 0:
        plt.figure(figsize=(8, 6))
        plt.bar(data[:, 0], data[:, 1], width=(data[1,0]-data[0,0]) if len(data)>1 else 0.1, color='blue', alpha=0.7)
        plt.xlabel("Positron Kinetic Energy (MeV)")
        plt.ylabel("Counts")
        plt.title("IBD Positron Kinetic Energy Spectrum")
        plt.grid(True, alpha=0.3)
        plt.savefig(plots_dir / "positron_Te_hist.png", dpi=300)
        plt.close()

    # 1b. IBD angular distributions
    for filename, xlabel, title, output, color in [
        ("ibd_cos_theta_e_hist.dat", r"$\cos\theta_{e^+}$", "IBD Positron Angular Distribution", "ibd_cos_theta_e_hist.png", "navy"),
        ("ibd_cos_theta_n_hist.dat", r"$\cos\theta_n$", "IBD Neutron Angular Distribution", "ibd_cos_theta_n_hist.png", "darkorange"),
    ]:
        data = load_dat(filename)
        if data is not None and len(data) > 0:
            plt.figure(figsize=(8, 6))
            plt.bar(data[:, 0], data[:, 1], width=(data[1,0]-data[0,0]) if len(data)>1 else 0.04, color=color, alpha=0.7)
            plt.xlabel(xlabel)
            plt.ylabel("Counts")
            plt.title(title)
            plt.grid(True, alpha=0.3)
            plt.savefig(plots_dir / output, dpi=300)
            plt.close()

    # 2a. Directional asymmetry: nu along -z vs +z
    data_mz = load_dat("asym_hist_nu_minus_z_trig3.dat")
    data_pz = load_dat("asym_hist_nu_plus_z_trig3.dat")
    if data_mz is not None and data_pz is not None and len(data_mz) > 0 and len(data_pz) > 0:
        width = (data_mz[1, 0] - data_mz[0, 0]) if len(data_mz) > 1 else 0.05
        plt.figure(figsize=(9, 6))
        plt.bar(data_mz[:, 0], data_mz[:, 1], width=width, alpha=0.55, color='navy',
                label=r'$\hat{\nu}$ along $-z$')
        plt.bar(data_pz[:, 0], data_pz[:, 1], width=width, alpha=0.55, color='darkorange',
                label=r'$\hat{\nu}$ along $+z$')
        mean_path = data_dir / "asym_mean_nu_dir_trig3.dat"
        if mean_path.exists():
            for line in mean_path.read_text().splitlines():
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split()
                if len(parts) >= 2:
                    tag, m = parts[0], float(parts[1])
                    color = 'navy' if 'minus' in tag else 'darkorange'
                    plt.axvline(m, color=color, linestyle='--', linewidth=1.2)
        plt.xlabel(r"Event asymmetry $A=(N_{pe}^{top}-N_{pe}^{bottom})/(N_{pe}^{top}+N_{pe}^{bottom})$")
        plt.ylabel("Counts")
        plt.title("Prompt positron asymmetry vs antineutrino direction (N_fired >= 3)")
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.savefig(plots_dir / "asym_hist_nu_dirs.png", dpi=300)
        plt.close()

    # 2. Asymmetry Histogram
    data = load_dat("asym_hist.dat")
    if data is not None and len(data) > 0:
        plt.figure(figsize=(8, 6))
        plt.bar(data[:, 0], data[:, 1], width=(data[1,0]-data[0,0]) if len(data)>1 else 0.05, color='green', alpha=0.7)
        plt.xlabel("Asymmetry (N_top - N_bottom) / N_total")
        plt.ylabel("Counts")
        plt.title("Light Collection Asymmetry")
        plt.grid(True, alpha=0.3)
        plt.savefig(plots_dir / "asym_hist.png", dpi=300)
        plt.close()

    # 3. Cherenkov Wavelength Histogram
    data = load_dat("cherenkov_lambda_hist.dat")
    if data is not None and len(data) > 0:
        plt.figure(figsize=(8, 6))
        plt.bar(data[:, 0], data[:, 1], width=(data[1,0]-data[0,0]) if len(data)>1 else 5.0, color='purple', alpha=0.7)
        plt.xlabel("Wavelength (nm)")
        plt.ylabel("Counts")
        plt.title("Cherenkov Photon Wavelength Spectrum")
        plt.grid(True, alpha=0.3)
        plt.savefig(plots_dir / "cherenkov_lambda_hist.png", dpi=300)
        plt.close()

    # 4. Mean Npe vs Te (uniform mono-energetic scan, trig3)
    data = load_dat("npe_vs_Te_mean_uniform_trig3.dat")
    if data is None or len(data) == 0:
        data = load_dat("npe_vs_Te_mean_trig3.dat")
    if data is not None and len(data) > 0:
        valid = data[:, 3] > 0 if data.shape[1] > 3 else np.ones(len(data), dtype=bool)
        if np.any(valid):
            plt.figure(figsize=(8, 6))
            plt.errorbar(data[valid, 0], data[valid, 1], yerr=data[valid, 2], fmt='o-', color='red', markersize=4)
            plt.xlabel("Positron Kinetic Energy (MeV)")
            plt.ylabel("Mean Npe")
            plt.title("Mean Photoelectrons vs Positron Energy (N_fired >= 3, uniform Te grid)")
            plt.grid(True, alpha=0.3)
            plt.savefig(plots_dir / "npe_vs_Te_mean.png", dpi=300)
            plt.close()

    # 5. Mean N fired PMTs vs Te
    data = load_dat("n_fired_vs_Te_mean.dat")
    if data is not None and len(data) > 0:
        valid = data[:, 3] > 0
        if np.any(valid):
            plt.figure(figsize=(8, 6))
            plt.errorbar(data[valid, 0], data[valid, 1], yerr=data[valid, 2], fmt='o-', color='orange', markersize=4)
            plt.xlabel("Positron Kinetic Energy (MeV)")
            plt.ylabel("Mean Number of Fired PMTs")
            plt.title("Mean Fired PMTs vs Positron Energy")
            plt.grid(True, alpha=0.3)
            plt.savefig(plots_dir / "n_fired_vs_Te_mean.png", dpi=300)
            plt.close()

    # 6. Npe histograms for Te slices
    plt.figure(figsize=(8, 6))
    colors = ['blue', 'green', 'red']
    for idx, slice_name in enumerate(["2_3", "5_6", "8_9"]):
        data = load_dat(f"npe_hist_Te_{slice_name}.dat")
        if data is not None and len(data) > 0:
            width = (data[1,0]-data[0,0]) if len(data)>1 else 50
            plt.bar(data[:, 0], data[:, 1], width=width, alpha=0.5, label=f"Te {slice_name.replace('_', '-')} MeV", color=colors[idx])
    plt.xlabel("Number of Photoelectrons (Npe)")
    plt.ylabel("Counts")
    plt.title("Npe Distributions for Positron Energy Slices")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.savefig(plots_dir / "npe_hist_Te_slices.png", dpi=300)
    plt.close()

    # 7. Neutron Lifetime
    plt.figure(figsize=(8, 6))
    data_cd = load_dat("lifetime_hist_Cd.dat")
    if data_cd is not None and len(data_cd) > 0:
        plt.plot(data_cd[:, 0], data_cd[:, 1], label="Cd 0.1%", drawstyle='steps-mid', color='blue')
    data_gd = load_dat("lifetime_hist_Gd.dat")
    if data_gd is not None and len(data_gd) > 0:
        plt.plot(data_gd[:, 0], data_gd[:, 1], label="Gd 0.1%", drawstyle='steps-mid', color='red')
    plt.xlabel(r"Neutron Lifetime ($\mu$s)")
    plt.ylabel("Counts")
    plt.title("Thermal Neutron Capture Lifetime")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.savefig(plots_dir / "neutron_lifetime.png", dpi=300)
    plt.close()

    # 8. Neutron Capture Npe -- two panels: log-Y step histogram + survival curve
    fig, (ax_hist, ax_surv) = plt.subplots(1, 2, figsize=(14, 6))

    data_cd = load_dat("npe_hist_log_Cd.dat")
    if data_cd is not None and len(data_cd) > 0:
        ax_hist.plot(data_cd[:, 0], data_cd[:, 1], label="Cd 0.1%",
                     drawstyle='steps-mid', color='blue', linewidth=1.2)
    data_gd = load_dat("npe_hist_log_Gd.dat")
    if data_gd is not None and len(data_gd) > 0:
        ax_hist.plot(data_gd[:, 0], data_gd[:, 1], label="Gd 0.1%",
                     drawstyle='steps-mid', color='red', linewidth=1.2)
    ax_hist.set_xlabel("Number of Photoelectrons ($N^{tot}_{pe}$)")
    ax_hist.set_ylabel("Counts")
    ax_hist.set_yscale('log')
    ax_hist.set_xlim(0, 300)
    ax_hist.set_title("Npe histogram (log Y)")
    ax_hist.legend()
    ax_hist.grid(True, which='both', alpha=0.3)

    surv_cd = load_dat("npe_survival_Cd.dat")
    if surv_cd is not None and len(surv_cd) > 0:
        ax_surv.plot(surv_cd[:, 0], surv_cd[:, 1], label="Cd 0.1%",
                     color='blue', linewidth=1.4)
    surv_gd = load_dat("npe_survival_Gd.dat")
    if surv_gd is not None and len(surv_gd) > 0:
        ax_surv.plot(surv_gd[:, 0], surv_gd[:, 1], label="Gd 0.1%",
                     color='red', linewidth=1.4)
    ax_surv.set_xlabel("Threshold $N^{tot}_{pe,thr}$")
    ax_surv.set_ylabel(r"$P(N^{tot}_{pe} \geq N^{tot}_{pe,thr})$")
    ax_surv.set_xlim(0, 250)
    ax_surv.set_ylim(0, 1)
    ax_surv.set_title("Survival curve (trigger efficiency)")
    ax_surv.legend()
    ax_surv.grid(True, alpha=0.3)

    fig.suptitle("Npe from Neutron Capture Gamma Cascade: Cd vs Gd", y=1.02)
    fig.tight_layout()
    fig.savefig(plots_dir / "neutron_capture_npe.png", dpi=300, bbox_inches='tight')
    plt.close(fig)

    # 9. Generated Optical Photons for Positron Percentiles
    fig, ax = plt.subplots(figsize=(12, 5))
    colors = {'p10': 'blue', 'p50': 'green', 'p90': 'red'}
    labels = {'p10': '0.845 MeV (p10)', 'p50': '2.227 MeV (p50)', 'p90': '4.203 MeV (p90)'}
    for tag in ["p10", "p50", "p90"]:
        data = load_dat(f"photons_hist_{tag}.dat")
        if data is not None and len(data) > 0:
            width = (data[1,0]-data[0,0]) if len(data)>1 else 100
            ax.bar(data[:, 0], data[:, 1], width=width, alpha=0.5, label=labels[tag], color=colors[tag])
    ax.set_xlabel("Number of Generated Optical Photons", fontsize=18)
    ax.set_ylabel("Counts", fontsize=18)
    ax.set_title("Generated Cherenkov Photons for Monoenergetic Positrons", fontsize=19)
    ax.tick_params(axis='both', labelsize=15)
    ax.legend(fontsize=16)
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(plots_dir / "photons_hist_percentiles.png", dpi=300)
    plt.close(fig)

    # 10. Neutron-capture prompt gamma spectrum (per dopant, dopant captures only)
    data_cd = load_dat("capture_gamma_spectrum_Cd.dat")
    data_gd = load_dat("capture_gamma_spectrum_Gd.dat")
    if (data_cd is not None and len(data_cd) > 0) or (data_gd is not None and len(data_gd) > 0):
        plt.figure(figsize=(8, 6))
        if data_cd is not None and len(data_cd) > 0:
            plt.plot(data_cd[:, 0], data_cd[:, 2], label="Cd ($^{114}$Cd*)",
                     drawstyle='steps-mid', color='blue', linewidth=1.2)
        if data_gd is not None and len(data_gd) > 0:
            plt.plot(data_gd[:, 0], data_gd[:, 2], label="Gd ($^{156,158}$Gd*)",
                     drawstyle='steps-mid', color='red', linewidth=1.2)
        plt.xlabel(r"Prompt capture $\gamma$ energy (MeV)")
        plt.ylabel("Fraction of capture gammas / bin")
        plt.yscale('log')
        plt.title("Neutron-capture prompt gamma spectrum (Geant4 G4NDL/HP)")
        plt.legend()
        plt.grid(True, which='both', alpha=0.3)
        plt.savefig(plots_dir / "capture_gamma_spectrum.png", dpi=300)
        plt.close()

    print(f"All plots saved to {plots_dir}")

if __name__ == "__main__":
    main()
