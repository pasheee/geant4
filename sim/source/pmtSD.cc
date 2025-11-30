#include "pmtSD.hh"
#include "G4Track.hh"
#include "G4StepPoint.hh"
#include "G4OpticalPhoton.hh"
#include "G4SystemOfUnits.hh"
#include "G4TouchableHandle.hh"
#include "G4VPhysicalVolume.hh"
#include "G4StepStatus.hh"
#include "G4UnitsTable.hh"
#include "G4ios.hh"

PMTSD::PMTSD(const G4String& name)
    : G4VSensitiveDetector(name),
      fEnergySum(0.0),
      fPhotonCount(0)
{}

PMTSD::~PMTSD() {}

void PMTSD::Initialize(G4HCofThisEvent*)
{
    fEnergySum = 0.0;
    fPhotonCount = 0;
    fSeenTracks.clear();
    fPhotonsPerPMT.clear();
    fEnergyPerPMT.clear();
}

G4bool PMTSD::ProcessHits(G4Step* step, G4TouchableHistory*)
{
    if (!step) return false;
    G4Track* track = step->GetTrack();
    if (!track) return false;

    // 1) Энергия (если есть)
    G4double edep = step->GetTotalEnergyDeposit();
    if (edep > 0.) {
        fEnergySum += edep;
        const G4StepPoint* post = step->GetPostStepPoint();
        if (post && post->GetTouchableHandle()) {
            G4int copy = post->GetTouchableHandle()->GetCopyNumber();
            fEnergyPerPMT[copy] += edep;
        }
    }

    // 2) Только оптические фотоны для счетчика
    if (track->GetDefinition() != G4OpticalPhoton::OpticalPhotonDefinition()) {
        return true;
    }

    G4int trackID = track->GetTrackID();

    // Если трек уже был учтён — пропускаем
    if (fSeenTracks.count(trackID) != 0) {
        return true;
    }

    // --- НОВАЯ ПРОСТАЯ ЛОГИКА: 
    // Если ProcessHits вызван для оптического фотона внутри этого чувствительного объёма,
    // считаем его 1 раз (без проверки pre->GetStepStatus()).
    fSeenTracks.insert(trackID);
    fPhotonCount++;

    // Получаем copy number для статистики по PMT (post preferred, then pre)
    G4int copyNumber = -1;
    const G4StepPoint* post = step->GetPostStepPoint();
    const G4StepPoint* pre  = step->GetPreStepPoint();
    if (post && post->GetTouchableHandle()) {
        copyNumber = post->GetTouchableHandle()->GetCopyNumber();
    } else if (pre && pre->GetTouchableHandle()) {
        copyNumber = pre->GetTouchableHandle()->GetCopyNumber();
    }
    if (copyNumber >= 0) fPhotonsPerPMT[copyNumber] += 1;

    return true;
}

void PMTSD::EndOfEvent(G4HCofThisEvent*)
{
    G4cout << "=== PMT Sensitive Detector Summary (" << GetName() << ") ===\n";
    G4cout << "Total energy deposited: " << G4BestUnit(fEnergySum, "Energy") << G4endl;
    G4cout << "Optical photons detected: " << fPhotonCount << G4endl;

    if (!fPhotonsPerPMT.empty()) {
        G4cout << "Photons per PMT (copy -> count):\n";
        for (const auto& kv : fPhotonsPerPMT) {
            G4cout << "  PMT " << kv.first << " : " << kv.second << G4endl;
        }
    }

    if (!fEnergyPerPMT.empty()) {
        G4cout << "Energy per PMT (copy -> energy):\n";
        for (const auto& kv : fEnergyPerPMT) {
            G4cout << "  PMT " << kv.first << " : " << G4BestUnit(kv.second, "Energy") << G4endl;
        }
    }
}
