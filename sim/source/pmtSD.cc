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
#include "G4RunManager.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "Randomize.hh"
#include <fstream>
#include <iomanip>

namespace {
// Пишем по событиям: Npe_top, Npe_bottom, асимметрия.
// Файл создаётся в текущей рабочей директории (обычно build/).
std::ofstream gAsymFile("pe_asymmetry.txt"); // trunc по умолчанию
bool gHeaderWritten = false;
}

PMTSD::PMTSD(const G4String& name)
    : G4VSensitiveDetector(name),
      fEnergySum(0.0),
      fPhotoElectronCount(0),
      fPhotoElectronCountTop(0),
      fPhotoElectronCountBottom(0)
{}

PMTSD::~PMTSD() {}

void PMTSD::Initialize(G4HCofThisEvent*)
{
    fEnergySum = 0.0;
    fPhotoElectronCount = 0;
    fPhotoElectronCountTop = 0;
    fPhotoElectronCountBottom = 0;
    fSeenTracks.clear();
    fPhotoElectronsPerPMT.clear();
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

    // 2) Только оптические фотоны для счётчика фотоэлектронов
    if (track->GetDefinition() != G4OpticalPhoton::OpticalPhotonDefinition()) {
        return true;
    }

    G4int trackID = track->GetTrackID();

    // Если трек уже был учтён — пропускаем
    if (fSeenTracks.count(trackID) != 0) {
        return true;
    }

    const G4StepPoint* pre  = step->GetPreStepPoint();
    if (!pre) return true;

    // Считаем попадание фотона в PMT один раз — в момент входа в чувствительный объём.
    // Это ближе к "фотон дошёл до фотокатода", чем подсчёт всех шагов в стекле.
    const bool entered = (pre->GetStepStatus() == fGeomBoundary);
    const bool createdInside = (track->GetCurrentStepNumber() == 1);
    if (!entered && !createdInside) {
        return true;
    }

    // Получаем copy number для статистики по PMT.
    G4int copyNumber = -1;
    if (pre->GetTouchableHandle()) {
        copyNumber = pre->GetTouchableHandle()->GetCopyNumber();
    }
    if (copyNumber < 0) return true;

    // Квантовая эффективность (QE): пробуем взять из MaterialPropertiesTable ("EFFICIENCY"),
    // иначе используем дефолт 0.28.
    G4double qe = 0.28;
    if (const auto* mat = pre->GetMaterial()) {
        if (auto* mpt = mat->GetMaterialPropertiesTable()) {
            if (auto* eff = mpt->GetProperty("EFFICIENCY")) {
                qe = eff->Value(track->GetTotalEnergy());
            }
        }
    }
    if (qe < 0.) qe = 0.;
    if (qe > 1.) qe = 1.;

    const bool detected = (G4UniformRand() < qe);

    // Считаем фотон обработанным и поглощаем его в PMT (детектирован или потерян).
    fSeenTracks.insert(trackID);
    track->SetTrackStatus(fStopAndKill);

    if (detected) {
        fPhotoElectronCount++;
        fPhotoElectronsPerPMT[copyNumber] += 1;

        // В вашей геометрии: top copy = 0..18, bottom copy = 19..37
        if (copyNumber < 19) {
            fPhotoElectronCountTop++;
        } else {
            fPhotoElectronCountBottom++;
        }
    }

    return true;
}

void PMTSD::EndOfEvent(G4HCofThisEvent*)
{
    G4int eventID = -1;
    if (auto* rm = G4RunManager::GetRunManager()) {
        if (const auto* evt = rm->GetCurrentEvent()) {
            eventID = evt->GetEventID();
        }
    }

    const G4int nTop = fPhotoElectronCountTop;
    const G4int nBottom = fPhotoElectronCountBottom;
    const G4int nTot = fPhotoElectronCount;

    G4double asym = 0.;
    if ((nTop + nBottom) > 0) {
        asym = (static_cast<G4double>(nTop) - static_cast<G4double>(nBottom)) /
               (static_cast<G4double>(nTop) + static_cast<G4double>(nBottom));
    }

    const G4int nFiredPMTs = fPhotoElectronsPerPMT.size();

    // Пишем строку в файл для последующей обработки (python/ROOT).
    if (gAsymFile) {
        if (!gHeaderWritten) {
            gAsymFile << "# event_id  Npe_top  Npe_bottom  Npe_total  asym  N_fired_PMTs\n";
            gHeaderWritten = true;
        }
        gAsymFile << eventID << " " << nTop << " " << nBottom << " " << nTot << " "
                  << std::setprecision(6) << asym << " " << nFiredPMTs << "\n";
        // Avoid flushing every event (slow for large statistics).
        if (eventID >= 0 && (eventID % 5000) == 0) {
            gAsymFile.flush();
        }
    }

    // Чтобы не заспамить консоль при больших статистиках, печатаем только первые события.
    if (eventID >= 0 && eventID < 5) {
        G4cout << "=== PMT Summary (" << GetName() << "), event " << eventID << " ===\n";
    G4cout << "Total energy deposited: " << G4BestUnit(fEnergySum, "Energy") << G4endl;
        G4cout << "Photoelectrons (QE applied): total=" << nTot
               << " top=" << nTop << " bottom=" << nBottom
               << " asym=" << asym << G4endl;

        if (!fPhotoElectronsPerPMT.empty()) {
            G4cout << "Photoelectrons per PMT (copy -> count):\n";
            for (const auto& kv : fPhotoElectronsPerPMT) {
                G4cout << "  PMT " << kv.first << " : " << kv.second << G4endl;
            }
        }
    }
}
