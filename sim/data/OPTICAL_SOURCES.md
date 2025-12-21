### Optical input data used in this project

This directory documents where the wavelength-dependent optical inputs come from.

#### Water refractive index \(n(\lambda)\)

- **Reference**: G. M. Hale and M. R. Querry, “Optical constants of water in the 200-nm to 200-µm wavelength region”, *Applied Optics* **12**, 555–563 (1973), DOI: `10.1364/AO.12.000555`.
- **Data source**: refractiveindex.info database (public domain, CC0)  
  GitHub: `https://github.com/polyanskiy/refractiveindex.info-database`  
  File: `database/data/main/H2O/nk/Hale.yml`

In the code, the table is used for 200–800 nm (and reversed to keep photon energy increasing).

#### Water absorption length \(L_{abs}(\lambda)\)

From the same Hale & Querry dataset, the extinction coefficient \(k(\lambda)\) is available.
We convert to an absorption length using:

\[
\\alpha(\\lambda)=\\frac{4\\pi k(\\lambda)}{\\lambda},\\qquad
L_{abs}(\\lambda)=\\frac{1}{\\alpha}=\\frac{\\lambda}{4\\pi k(\\lambda)}
\]

#### PMT quantum efficiency \(QE(\lambda)\)

- **Data source**: WCSim (The Water Cherenkov Simulator), MIT license  
  Repo: `https://github.com/WCSim/WCSim`  
  QE curve used: `PMT10inchHQE::GetQE` in `src/WCSimPMTObject.cc`

In the code this curve is used as Geant4 `EFFICIENCY` vs photon energy (280–660 nm).


