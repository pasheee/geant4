
# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

`## Project Overview

Geant4-based simulation of a water Cherenkov detector for reactor antineutrino detection via Inverse Beta Decay (IBD). The detector uses a doped-water target (Cd or Gd), surrounded by a pure-water buffer, instrumented with 38 PMTs. The project includes a full Python/LaTeX analysis pipeline for producing publication plots.

## Build

```bash
cd sim/build
cmake ..
make -j$(nproc)
```

The executable is `sim/build/sim`. Output data files (`.txt`) are written to `sim/build/` at runtime.

Run interactively (with GUI):
```bash
cd sim/build && ./sim
```

Run in batch mode with a macro:
```bash
cd sim/build && ./sim ../macros/batch_nirs.mac
```

Set random seed via environment variable (defaults to system time):
```bash
SIM_SEED=42 ./sim ../macros/batch_nirs.mac
```

## Full Analysis Pipeline

```bash
# 100k events across all channels in parallel
./run_100k.sh

# 1M events
./run_1M.sh
```

These scripts activate the `g4env` conda environment, run 6 simulations in parallel (IBD+NIRS, neutron Cd, neutron Gd, positron at p10/p50/p90 energies), collect outputs to `sim/build/`, then run Python analysis.

```bash
# Generate histogram .dat files for LaTeX (pgfplots)
python NIRS/make_plots_data.py

# Generate PNG plots
python NIRS/make_pngs.py
```

## Code Architecture

All C++ source lives in `sim/source/*.cc` with headers in `sim/include/*.hh`. CMake globs both directories automatically — adding new `.cc`/`.hh` files requires no CMakeLists.txt edits.

### Key classes

| File | Class | Role |
|---|---|---|
| `sim.cc` | — | Entry point: initializes RunManager, selects batch vs. interactive mode |
| `construction.cc` | `DetectorConstruction` | Full geometry: world → steel tank → water buffer → PMMA vessel → doped-water target + 38 PMTs in concentric rings |
| `physics.cc` | `PhysicsList` | Registers Em, Optical, Decay, HadronElasticHP, FTFP\_BERT\_HP, Ion physics |
| `generator.cc` | `PrimaryGeneratorAction` | Two modes: `mono` (fixed particle/energy) and `ibd` (reactor antineutrino spectrum → positron) |
| `action.cc` | `RunAction`, `EventAction`, `SteppingAction` | Per-run file management; per-event photon counting; per-step Cherenkov and neutron capture tracking |
| `sensitive.cc` | `SensitiveDetector` | Records energy deposit in doped-water target |
| `pmtSD.cc` | `PMTSD` | Counts optical photons on PMT photocathodes; tracks top/bottom asymmetry |

### Detector geometry

- **World:** 2×2×2 m³ air
- **Steel tank:** R=632 mm, H=1300 mm (stainless steel)
- **Water buffer:** fills tank, has wavelength-dependent refractive index and absorption (200–800 nm)
- **PMMA vessel:** R≈600 mm, H≈700 mm
- **Doped-water target:** R<590 mm — dopant switched at runtime via `/det/dopant Cd|Gd|None`
- **PMT array (38 total):** top cap (copy #0–18) and bottom cap (copy #19–37), each with 1 center + 6 mid-ring + 12 outer-ring PMTs

### Output files (written to `sim/build/`)

| File | Content |
|---|---|
| `pe_asymmetry.txt` | Per-event: `event_id Npe_top Npe_bottom Npe_total asymmetry N_fired_PMTs` |
| `ibd_positron_spectrum.txt` | Per-event: `event_id Enu_MeV Te+_MeV` |
| `cherenkov_spectrum.txt` | One photon energy per line (eV); enabled via `/analysis/writeCherenkovSpectrum true` |
| `neutron_lifetime_Cd.txt` / `_Gd.txt` | Neutron capture times (ns) |
| `photons_p10/p50/p90.txt` | Cherenkov photon counts for fixed-energy positron runs |
| `photons_per_event.txt` | `event_id N_photons` |

### Geant4 macro commands

| Command | Values | Effect |
|---|---|---|
| `/gen/mode` | `mono`, `ibd` | Particle generation mode |
| `/gen/particle` | `e+`, `e-`, `neutron`, … | Particle type (mono mode) |
| `/gen/monoEnergy` | value + unit | Kinetic energy (mono mode) |
| `/det/dopant` | `Cd`, `Gd`, `None` | Target dopant |
| `/gen/frac235U` etc. | 0–1 | Fission fractions for IBD spectrum |
| `/analysis/writeCherenkovSpectrum` | `true`/`false` | Enable Cherenkov spectrum output |

### Analysis pipeline

`NIRS/make_plots_data.py` reads simulation `.txt` files and writes `.dat` histogram files into `NIRS/data/` for use with LaTeX pgfplots. `NIRS/make_pngs.py` generates matplotlib PNGs into `NIRS/plots/`. The main document is `NIRS/main.tex`; the presentation is `NIRS/presentation.tex`.
