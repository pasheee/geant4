#include "sensitive.hh"
#include "G4OpticalPhoton.hh"
#include "G4Track.hh"
#include "G4StepPoint.hh"
#include "G4VProcess.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"

SensitiveDetector::SensitiveDetector(const G4String& name)
    : G4VSensitiveDetector(name),
      fTotalEnergyDeposed(0.),
      fPhotonCount(0)
{}

SensitiveDetector::~SensitiveDetector() {}

void SensitiveDetector::Initialize(G4HCofThisEvent*) {
    fTotalEnergyDeposed = 0.;
    fPhotonCount = 0;

    // Множество ID треков, чтобы не считать один фотон много раз
    fSeenTracks.clear();
}

G4bool SensitiveDetector::ProcessHits(G4Step* step, G4TouchableHistory*) {
    auto track = step->GetTrack();

    // === 1. Считаем энергию депозиции (как раньше) ===
    G4double edep = step->GetTotalEnergyDeposit();
    if (edep > 0.) fTotalEnergyDeposed += edep;

    // === 2. Считаем фотоны ===
    if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {

        G4int trackID = track->GetTrackID();

        // Если уже считали этот трек — пропускаем
        if (fSeenTracks.count(trackID) == 0) {

            // Берём точки шага
            auto pre = step->GetPreStepPoint();
            auto post = step->GetPostStepPoint();

            // ====== Условие 1: фотон ВОШЁЛ в sensitive volume ======
            bool entered =
                (pre->GetStepStatus() == fGeomBoundary);

            // ====== Условие 2: фотон СОЗДАН внутри sensitive volume ======
            bool createdInside =
                (track->GetCurrentStepNumber() == 1);  

            if (entered || createdInside) {
                fPhotonCount++;
                fSeenTracks.insert(trackID);
            }
        }
    }

    return true;
}

void SensitiveDetector::EndOfEvent(G4HCofThisEvent*) {
    // Avoid spamming output for large statistics runs: print only for first few events.
    G4int eventID = -1;
    if (auto* rm = G4RunManager::GetRunManager()) {
        if (const auto* evt = rm->GetCurrentEvent()) {
            eventID = evt->GetEventID();
        }
    }

    if (eventID >= 0 && eventID < 5) {
        G4cout << "Total energy deposited in " << GetName()
               << ": " << G4BestUnit(fTotalEnergyDeposed, "Energy") << G4endl;

        G4cout << "Optical photons detected in " << GetName()
               << ": " << fPhotonCount << G4endl;
    }
}
