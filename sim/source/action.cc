#include "action.hh"
#include "generator.hh"
#include "G4RunManager.hh"
#include "G4Event.hh"
#include "G4Step.hh"
#include "G4OpticalPhoton.hh"
#include "G4Track.hh"
#include "G4SystemOfUnits.hh"
#include "G4GenericMessenger.hh"
#include "G4ios.hh"
#include <iomanip>

RunAction::RunAction()
    : fPhotonCount(0),
      fWriteCherenkovSpectrum(false),
      fCherenkovSpectrumFile("chernkov_spectrum.txt"),
      fCherenkovOut(),
      fMessenger(nullptr)
{
    // Controls for long runs: by default do NOT write cherenkov_spectrum.txt,
    // because it can become extremely large.
    fMessenger = new G4GenericMessenger(this, "/analysis/", "Analysis / output control");
    fMessenger->DeclareProperty("writeCherenkovSpectrum", fWriteCherenkovSpectrum,
                                "If true: write Cerenkov photon energies (eV) to a text file");
    fMessenger->DeclareProperty("cherenkovSpectrumFile", fCherenkovSpectrumFile,
                                "Output filename for Cerenkov photon spectrum");
}

RunAction::~RunAction()
{
    if (fCherenkovOut.is_open()) fCherenkovOut.close();
    delete fMessenger;
}

void RunAction::BeginOfRunAction(const G4Run*) {
    fPhotonCount = 0;
    G4cout << "=== Run started ===" << G4endl;

    if (fWriteCherenkovSpectrum) {
        fCherenkovOut.open(fCherenkovSpectrumFile, std::ios::out);
        if (!fCherenkovOut) {
            G4cerr << "[RunAction] Cannot open '" << fCherenkovSpectrumFile
                   << "' for writing. Disabling spectrum output." << G4endl;
            fWriteCherenkovSpectrum = false;
        } else {
            fCherenkovOut << "# Cherenkov photon energy (eV), one per line\n";
        }
    }
}

void RunAction::EndOfRunAction(const G4Run*) {
    G4cout << "=== Run finished ===" << G4endl;
    G4cout << "Total number of optical photons produced: " << fPhotonCount << G4endl;

    if (fCherenkovOut.is_open()) {
        fCherenkovOut.flush();
        fCherenkovOut.close();
    }
}

void RunAction::AddPhoton() { fPhotonCount++; }

void RunAction::WriteCherenkovPhotonEnergyEV(G4double energyEV)
{
    if (!fWriteCherenkovSpectrum) return;
    if (!fCherenkovOut.is_open()) return;
    fCherenkovOut << std::setprecision(8) << energyEV << "\n";
}

SteppingAction::SteppingAction(RunAction* runAction) : fRunAction(runAction) {}
SteppingAction::~SteppingAction() {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
    auto track = step->GetTrack();

    if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
        if (track->GetCurrentStepNumber() == 1) {
            const G4VProcess* creatorProc = track->GetCreatorProcess();
            if (creatorProc) {
                const G4String& procName = creatorProc->GetProcessName();
                if (procName == "Cerenkov") {
                    fRunAction->AddPhoton();

                    G4double E = track->GetTotalEnergy() / eV;
                    fRunAction->WriteCherenkovPhotonEnergyEV(E);
                }
            }
        }
    }
}

ActionInitialization::ActionInitialization() {}
ActionInitialization::~ActionInitialization() {}

void ActionInitialization::Build() const {
    auto gen = new PrimaryGenerator();
    SetUserAction(gen);

    auto runAction = new RunAction();
    SetUserAction(runAction);

    auto stepping = new SteppingAction(runAction);
    SetUserAction(stepping);
}
