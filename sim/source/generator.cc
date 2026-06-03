#include "generator.hh"
#include "G4Event.hh"
#include "G4ParticleDefinition.hh"
#include "G4GenericMessenger.hh"
#include "G4PhysicalConstants.hh"
#include "G4ios.hh"
#include "Randomize.hh"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>

namespace {
// Для проверки разыгрываемого спектра IBD: пишем Enu и T(e+) по событиям.
std::ofstream gIbdFile("ibd_positron_spectrum.txt"); // trunc по умолчанию
bool gIbdHeaderWritten = false;
std::ofstream gIbdKinematicsFile("ibd_kinematics.txt");
bool gIbdKinematicsHeaderWritten = false;

G4ThreeVector SampleIsotropicDirection()
{
    const G4double u = 2.0 * G4UniformRand() - 1.0;      // cos(theta)
    const G4double phi = 2.0 * pi * G4UniformRand();
    const G4double s2 = 1.0 - u * u;
    const G4double s = (s2 > 0.) ? std::sqrt(s2) : 0.;
    return {s * std::cos(phi), s * std::sin(phi), u};
}

G4ThreeVector DirectionFromAxis(G4ThreeVector axis, G4double cosTheta, G4double phi)
{
    if (axis.mag2() <= 0.) axis = G4ThreeVector(0., 0., 1.);
    axis = axis.unit();
    cosTheta = std::max(-1.0, std::min(1.0, cosTheta));
    const G4double sinTheta2 = 1.0 - cosTheta * cosTheta;
    const G4double sinTheta = (sinTheta2 > 0.) ? std::sqrt(sinTheta2) : 0.;

    const G4ThreeVector ref = (std::abs(axis.z()) < 0.9)
                                  ? G4ThreeVector(0., 0., 1.)
                                  : G4ThreeVector(0., 1., 0.);
    const G4ThreeVector u = ref.cross(axis).unit();
    const G4ThreeVector v = axis.cross(u).unit();
    return (cosTheta * axis + sinTheta * (std::cos(phi) * u + std::sin(phi) * v)).unit();
}
} // namespace

PrimaryGenerator::PrimaryGenerator()
    : fMode("ibd"),
      fParticleName("e+"),
      fMonoKineticEnergy(6. * MeV),
      fPosition(0., 0., 0.),
      fIsotropicDirection(true),
      fDirection(0., 0., 1.),
      fRandomizePos(true),
      fNuEmin(1.806 * MeV),
      fNuEmax(10.0 * MeV),
      fFrac235U(0.56),
      fFrac238U(0.08),
      fFrac239Pu(0.30),
      fFrac241Pu(0.06),
      fIbdAngularModel("isotropic"),
      fIbdNuDir(0., 0., -1.),
      fIbdEmitNeutron(false),
      fParticleGun(new G4ParticleGun(1)),
      fMessenger(nullptr),
      fIbdWmax(0.0),
      fIbdWmaxValid(false),
      fCachedNuEmin(-1.0),
      fCachedNuEmax(-1.0),
      fCachedFrac235U(-1.0),
      fCachedFrac238U(-1.0),
      fCachedFrac239Pu(-1.0),
      fCachedFrac241Pu(-1.0)
{
    SetupMessenger();
    NormalizeFissionFractions();
}
PrimaryGenerator::~PrimaryGenerator() {
    delete fMessenger;
    delete fParticleGun;
}

void PrimaryGenerator::GeneratePrimaries(G4Event *anEvent) {
    G4ParticleTable *particleTable = G4ParticleTable::GetParticleTable();
    G4ParticleDefinition *particle = particleTable->FindParticle(fParticleName);
    if (!particle) {
        G4cerr << "[PrimaryGenerator] Unknown particle '" << fParticleName
               << "'. Fallback to e+." << G4endl;
        particle = particleTable->FindParticle("e+");
    }

    // Position
    G4ThreeVector pos = fPosition;
    if (fMode == "ibd" || fRandomizePos) {
        // Равномерный сэмплинг внутри центральной колбы (PMMA vessel: inner R=590mm, half-height=350mm)
        // Берём чуть меньше, чтобы точно не вылезти за границу
        G4double rMax = 589.0 * mm;
        G4double zMax = 349.0 * mm;
        G4double r = rMax * std::sqrt(G4UniformRand());
        G4double phi = 2.0 * pi * G4UniformRand();
        G4double z = (2.0 * G4UniformRand() - 1.0) * zMax;
        pos = G4ThreeVector(r * std::cos(phi), r * std::sin(phi), z);
    }
    fParticleGun->SetParticlePosition(pos);

    // Direction
    G4ThreeVector dir = fDirection;
    if (fIsotropicDirection) {
        dir = SampleIsotropicDirection();
    }
    if (dir.mag2() > 0) dir = dir.unit();
    else dir = G4ThreeVector(0., 0., 1.);
    fParticleGun->SetParticleMomentumDirection(dir);

    // Energy
    NormalizeFissionFractions();
    G4double kineticEnergy = fMonoKineticEnergy;

    if (fMode == "ibd") {
        const G4double Enu = SampleIbdNuEnergy();
        G4ThreeVector nuDir = fIbdNuDir;
        if (nuDir.mag2() > 0.) nuDir = nuDir.unit();
        else nuDir = G4ThreeVector(0., 0., -1.);

        const G4double delta = neutron_mass_c2 - proton_mass_c2;
        const G4double Ee0 = Enu - delta;               // total e+ energy at O(1)
        G4double Ee = Ee0;
        G4double cosThetaE = nuDir.dot(dir);

        if (fIbdAngularModel == "vogelBeacomTable") {
            cosThetaE = SampleIbdCosTheta(Enu);
            const G4double phi = 2.0 * pi * G4UniformRand();
            dir = DirectionFromAxis(nuDir, cosThetaE, phi);

            const G4double pe02 = Ee0 * Ee0 - electron_mass_c2 * electron_mass_c2;
            if (pe02 > 0.) {
                const G4double pe0 = std::sqrt(pe02);
                const G4double ve0 = pe0 / Ee0;
                const G4double avgNucleonMass = 0.5 * (proton_mass_c2 + neutron_mass_c2);
                const G4double y2 = 0.5 * (delta * delta - electron_mass_c2 * electron_mass_c2);
                Ee = Ee0 * (1.0 - (Enu / avgNucleonMass) * (1.0 - ve0 * cosThetaE))
                     - y2 / avgNucleonMass;
            }
        } else if (fIbdAngularModel != "isotropic") {
            G4cerr << "[PrimaryGenerator] Unknown IBD angular model '" << fIbdAngularModel
                   << "'. Fallback to isotropic/fixed generator direction." << G4endl;
        }

        if (Ee < electron_mass_c2) Ee = electron_mass_c2;
        kineticEnergy = Ee - electron_mass_c2;
        if (kineticEnergy < 0.) kineticEnergy = 0.;

        const G4double pe2 = Ee * Ee - electron_mass_c2 * electron_mass_c2;
        const G4double pe = (pe2 > 0.) ? std::sqrt(pe2) : 0.;
        G4ThreeVector neutronMomentum = Enu * nuDir - pe * dir;
        G4ThreeVector neutronDir = neutronMomentum.mag2() > 0. ? neutronMomentum.unit() : nuDir;
        const G4double neutronKineticEnergy = std::max(0.0, Enu - delta - Ee);
        const G4double cosThetaN = nuDir.dot(neutronDir);

        // Запишем разыгранные значения для контроля спектра
        if (gIbdFile) {
            if (!gIbdHeaderWritten) {
                gIbdFile << "# event_id  Enu_MeV  Te+_MeV\n";
                gIbdHeaderWritten = true;
            }
            gIbdFile << anEvent->GetEventID() << " "
                     << (Enu / MeV) << " "
                     << (kineticEnergy / MeV) << "\n";
            // Avoid flushing every event (slow for large statistics).
            if ((anEvent->GetEventID() % 5000) == 0) {
                gIbdFile.flush();
            }
        }

        if (gIbdKinematicsFile) {
            if (!gIbdKinematicsHeaderWritten) {
                gIbdKinematicsFile
                    << "# event_id Enu_MeV Ee_MeV Te_MeV cosThetaE thetaE_deg "
                    << "eDirX eDirY eDirZ Tn_keV cosThetaN thetaN_deg nDirX nDirY nDirZ\n";
                gIbdKinematicsHeaderWritten = true;
            }
            const G4double thetaE = std::acos(std::max(-1.0, std::min(1.0, cosThetaE)));
            const G4double thetaN = std::acos(std::max(-1.0, std::min(1.0, cosThetaN)));
            gIbdKinematicsFile << anEvent->GetEventID() << " "
                               << std::setprecision(8)
                               << (Enu / MeV) << " "
                               << (Ee / MeV) << " "
                               << (kineticEnergy / MeV) << " "
                               << cosThetaE << " "
                               << (thetaE / deg) << " "
                               << dir.x() << " " << dir.y() << " " << dir.z() << " "
                               << (neutronKineticEnergy / keV) << " "
                               << cosThetaN << " "
                               << (thetaN / deg) << " "
                               << neutronDir.x() << " " << neutronDir.y() << " " << neutronDir.z()
                               << "\n";
            if ((anEvent->GetEventID() % 5000) == 0) {
                gIbdKinematicsFile.flush();
            }
        }

        fParticleGun->SetParticleMomentumDirection(dir);
        fParticleGun->SetParticleEnergy(kineticEnergy);
        fParticleGun->SetParticleDefinition(particleTable->FindParticle("e+"));
        fParticleGun->GeneratePrimaryVertex(anEvent);

        if (fIbdEmitNeutron) {
            if (auto* neutron = particleTable->FindParticle("neutron")) {
                fParticleGun->SetParticleMomentumDirection(neutronDir);
                fParticleGun->SetParticleEnergy(neutronKineticEnergy);
                fParticleGun->SetParticleDefinition(neutron);
                fParticleGun->GeneratePrimaryVertex(anEvent);
            }
        }
        return;
    }

    fParticleGun->SetParticleEnergy(kineticEnergy);
    fParticleGun->SetParticleDefinition(particle);

    fParticleGun->GeneratePrimaryVertex(anEvent);
}

void PrimaryGenerator::SetupMessenger()
{
    fMessenger = new G4GenericMessenger(this, "/gen/", "Primary generator control");
    fMessenger->DeclareProperty("mode", fMode, "Generator mode: mono | ibd");
    fMessenger->DeclareProperty("particle", fParticleName, "Particle name (e+, e-, ...)");
    fMessenger->DeclarePropertyWithUnit("monoEnergy", "MeV", fMonoKineticEnergy,
                                        "Mono-energetic kinetic energy (used when mode=mono)");
    fMessenger->DeclareProperty("isotropic", fIsotropicDirection,
                               "If true: isotropic momentum direction");
    fMessenger->DeclareProperty("randomizePos", fRandomizePos,
                               "If true: uniform position sampling inside PMMA vessel");
    fMessenger->DeclareProperty("dir", fDirection,
                               "Momentum direction vector (used when isotropic=false)");

    fMessenger->DeclarePropertyWithUnit("nuEmin", "MeV", fNuEmin,
                                        "Minimum antineutrino energy for IBD sampling");
    fMessenger->DeclarePropertyWithUnit("nuEmax", "MeV", fNuEmax,
                                        "Maximum antineutrino energy for IBD sampling");

    fMessenger->DeclareProperty("frac235U", fFrac235U, "Fission fraction weight for 235U");
    fMessenger->DeclareProperty("frac238U", fFrac238U, "Fission fraction weight for 238U");
    fMessenger->DeclareProperty("frac239Pu", fFrac239Pu, "Fission fraction weight for 239Pu");
    fMessenger->DeclareProperty("frac241Pu", fFrac241Pu, "Fission fraction weight for 241Pu");

    fMessenger->DeclareProperty("ibdAngularModel", fIbdAngularModel,
                                "IBD angular model: isotropic | vogelBeacomTable");
    fMessenger->DeclareProperty("ibdNuDir", fIbdNuDir,
                                "Incoming antineutrino direction for IBD angular sampling");
    fMessenger->DeclareProperty("ibdEmitNeutron", fIbdEmitNeutron,
                                "If true: generate the correlated IBD neutron primary");
}

void PrimaryGenerator::NormalizeFissionFractions()
{
    const G4double old235 = fFrac235U;
    const G4double old238 = fFrac238U;
    const G4double old239 = fFrac239Pu;
    const G4double old241 = fFrac241Pu;

    const G4double sum = fFrac235U + fFrac238U + fFrac239Pu + fFrac241Pu;
    if (sum <= 0.) {
        fFrac235U = 1.0;
        fFrac238U = 0.0;
        fFrac239Pu = 0.0;
        fFrac241Pu = 0.0;
    } else {
        fFrac235U /= sum;
        fFrac238U /= sum;
        fFrac239Pu /= sum;
        fFrac241Pu /= sum;
    }

    const auto changed = [](G4double a, G4double b) -> bool {
        return std::abs(a - b) > 1e-12;
    };
    if (changed(fFrac235U, old235) || changed(fFrac238U, old238) ||
        changed(fFrac239Pu, old239) || changed(fFrac241Pu, old241)) {
        fIbdWmaxValid = false;
    }
}

G4double PrimaryGenerator::ReactorFluxPerFission(G4double Enu) const
{
    // Huber-Mueller параметризация (форма): S(E) = exp( sum_{k=0..5} a_k E^k ), E в MeV.
    // Коэффициенты: Huber (2011) для 235U/239Pu/241Pu; Mueller (2011) для 238U.
    // Важно: нам нужна только форма для разыгрывания спектра, абсолютная нормировка не используется.
    static constexpr std::array<G4double, 6> a235U = {
        4.367, -4.577, 2.100, -0.5294, 0.06186, -0.002777};
    static constexpr std::array<G4double, 6> a239Pu = {
        4.757, -5.392, 2.563, -0.6596, 0.07820, -0.003536};
    static constexpr std::array<G4double, 6> a241Pu = {
        2.990, -2.882, 1.278, -0.3343, 0.03905, -0.001754};
    static constexpr std::array<G4double, 6> a238U = {
        4.833, -5.392, 2.563, -0.6596, 0.07820, -0.003536};

    const G4double x = Enu / MeV;
    auto spec = [x](const std::array<G4double, 6>& a) -> G4double {
        G4double p = 0.0;
        G4double xp = 1.0;
        for (std::size_t i = 0; i < a.size(); ++i) {
            p += a[i] * xp;
            xp *= x;
        }
        return std::exp(p);
    };

    return fFrac235U * spec(a235U) +
           fFrac238U * spec(a238U) +
           fFrac239Pu * spec(a239Pu) +
           fFrac241Pu * spec(a241Pu);
}

G4double PrimaryGenerator::IbdWeight(G4double Enu) const
{
    // Спектр событий IBD ~ phi(Enu) * sigma(Enu).
    // Берём форму sigma ~ pe * Ee (Vogel & Beacom, 1999), без радиационных/рекойл поправок.
    const G4double delta = neutron_mass_c2 - proton_mass_c2;
    const G4double Ee = Enu - delta; // total positron energy
    if (Ee <= electron_mass_c2) return 0.0;
    const G4double pe2 = Ee * Ee - electron_mass_c2 * electron_mass_c2;
    if (pe2 <= 0.) return 0.0;
    const G4double pe = std::sqrt(pe2);

    const G4double flux = ReactorFluxPerFission(Enu);
    return flux * pe * Ee;
}

void PrimaryGenerator::RecomputeIbdWmax()
{
    if (fNuEmax <= fNuEmin) {
        fNuEmin = 1.806 * MeV;
        fNuEmax = 10.0 * MeV;
    }

    const int nScan = 5000;
    G4double wmax = 0.0;
    for (int i = 0; i <= nScan; ++i) {
        const G4double Enu = fNuEmin + (fNuEmax - fNuEmin) * (static_cast<G4double>(i) / nScan);
        const G4double w = IbdWeight(Enu);
        if (w > wmax) wmax = w;
    }
    if (wmax <= 0.) wmax = 1.0;
    fIbdWmax = wmax;
    fIbdWmaxValid = true;
}

G4double PrimaryGenerator::SampleIbdNuEnergy()
{
    const auto changed = [](G4double a, G4double b) -> bool {
        return std::abs(a - b) > 1e-12;
    };

    const bool paramsChanged =
        (!fIbdWmaxValid) ||
        changed(fCachedNuEmin, fNuEmin) ||
        changed(fCachedNuEmax, fNuEmax) ||
        changed(fCachedFrac235U, fFrac235U) ||
        changed(fCachedFrac238U, fFrac238U) ||
        changed(fCachedFrac239Pu, fFrac239Pu) ||
        changed(fCachedFrac241Pu, fFrac241Pu);

    if (paramsChanged) {
        RecomputeIbdWmax();
        fCachedNuEmin = fNuEmin;
        fCachedNuEmax = fNuEmax;
        fCachedFrac235U = fFrac235U;
        fCachedFrac238U = fFrac238U;
        fCachedFrac239Pu = fFrac239Pu;
        fCachedFrac241Pu = fFrac241Pu;
    }

    // rejection sampling
    for (int tries = 0; tries < 2000000; ++tries) {
        const G4double Enu = fNuEmin + (fNuEmax - fNuEmin) * G4UniformRand();
        const G4double w = IbdWeight(Enu);
        if (w <= 0.) continue;
        if (G4UniformRand() < (w / fIbdWmax)) {
            return Enu;
        }
    }

    // fallback (не должно происходить при нормальных параметрах)
    return fNuEmin;
}

G4double PrimaryGenerator::IbdAngularWeight(G4double Enu, G4double cosTheta) const
{
    cosTheta = std::max(-1.0, std::min(1.0, cosTheta));

    // Vogel & Beacom, Phys. Rev. D 60, 053003 (1999), Eqs. 13-15.
    // Overall constants cancel in the CDF, so only the positive relative shape is used.
    static constexpr G4double f = 1.0;
    static constexpr G4double g = 1.26;
    static constexpr G4double f2 = 3.706;

    const G4double delta = neutron_mass_c2 - proton_mass_c2;
    const G4double Ee0 = Enu - delta;
    if (Ee0 <= electron_mass_c2) return 0.0;

    const G4double pe02 = Ee0 * Ee0 - electron_mass_c2 * electron_mass_c2;
    if (pe02 <= 0.) return 0.0;
    const G4double pe0 = std::sqrt(pe02);
    const G4double ve0 = pe0 / Ee0;
    if (ve0 <= 0.) return 0.0;

    const G4double avgNucleonMass = 0.5 * (proton_mass_c2 + neutron_mass_c2);
    const G4double y2 = 0.5 * (delta * delta - electron_mass_c2 * electron_mass_c2);
    const G4double Ee1 = Ee0 * (1.0 - (Enu / avgNucleonMass) * (1.0 - ve0 * cosTheta))
                       - y2 / avgNucleonMass;
    if (Ee1 <= electron_mass_c2) return 0.0;

    const G4double pe12 = Ee1 * Ee1 - electron_mass_c2 * electron_mass_c2;
    if (pe12 <= 0.) return 0.0;
    const G4double pe1 = std::sqrt(pe12);
    const G4double ve1 = pe1 / Ee1;

    const G4double me2 = electron_mass_c2 * electron_mass_c2;
    const G4double gamma =
        2.0 * (f + f2) * g *
            ((2.0 * Ee0 + delta) * (1.0 - ve0 * cosTheta) - me2 / Ee0) +
        (f * f + g * g) *
            (delta * (1.0 + ve0 * cosTheta) + me2 / Ee0) +
        (f * f + 3.0 * g * g) *
            ((Ee0 + delta) * (1.0 - cosTheta / ve0) - delta) +
        (f * f - g * g) *
            ((Ee0 + delta) * (1.0 - cosTheta / ve0) - delta) * ve0 * cosTheta;

    const G4double leading =
        ((f * f + 3.0 * g * g) + (f * f - g * g) * ve1 * cosTheta) * Ee1 * pe1;
    const G4double recoil = (gamma / avgNucleonMass) * Ee0 * pe0;
    const G4double weight = leading - recoil;
    return std::max(0.0, weight);
}

G4double PrimaryGenerator::SampleIbdCosTheta(G4double Enu) const
{
    static constexpr int nBins = 512;
    const G4double cMin = -1.0;
    const G4double cMax = 1.0;
    const G4double dc = (cMax - cMin) / nBins;

    G4double cumulative[nBins];
    G4double total = 0.0;
    for (int i = 0; i < nBins; ++i) {
        const G4double cMid = cMin + (i + 0.5) * dc;
        total += IbdAngularWeight(Enu, cMid) * dc;
        cumulative[i] = total;
    }

    if (total <= 0.) {
        return 2.0 * G4UniformRand() - 1.0;
    }

    const G4double target = G4UniformRand() * total;
    for (int i = 0; i < nBins; ++i) {
        if (target <= cumulative[i]) {
            const G4double c0 = cMin + i * dc;
            return c0 + G4UniformRand() * dc;
        }
    }
    return cMax;
}
