import numpy as np
import matplotlib.pyplot as plt

# Константа: hc = 1239.841984 eV·nm
hc = 1239.841984

# Загружаем данные
energies_eV = np.loadtxt("chernkov_spectrum.txt")

# Переводим в длины волн (нм)
wavelengths_nm = hc / energies_eV

# Строим гистограмму
plt.figure(figsize=(8, 5))
plt.hist(wavelengths_nm, bins=100, color='royalblue', alpha=0.7)
plt.xlabel("Wavelength (nm)")
plt.ylabel("Photon count")
plt.title("Cherenkov photon spectrum from Geant4")
plt.grid(True, alpha=0.3)
plt.show()
