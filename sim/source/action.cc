#include "action.hh"
#include "generator.hh"
#include "G4RunManager.hh"
#include "G4Event.hh"
#include "G4Step.hh"
#include "G4OpticalPhoton.hh"
#include "G4Neutron.hh"
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
      fNeutronLifetimeFile("neutron_lifetime_Cd.txt"),
      fNeutronLifetimeOut(),
      fPhotonsPerEventFile("photons_per_event.txt"),
      fPhotonsPerEventOut(),
      fMessenger(nullptr)
{
    // Controls for long runs: by default do NOT write cherenkov_spectrum.txt,
    // because it can become extremely large.
    fMessenger = new G4GenericMessenger(this, "/analysis/", "Analysis / output control");
    fMessenger->DeclareProperty("writeCherenkovSpectrum", fWriteCherenkovSpectrum,
                                "If true: write Cerenkov photon energies (eV) to a text file");
    fMessenger->DeclareProperty("cherenkovSpectrumFile", fCherenkovSpectrumFile,
                                "Output filename for Cerenkov photon spectrum");
    fMessenger->DeclareProperty("neutronLifetimeFile", fNeutronLifetimeFile,
                                "Output filename for neutron lifetime data");
    fMessenger->DeclareProperty("photonsPerEventFile", fPhotonsPerEventFile,
                                "Output filename for number of photons per event");
}

RunAction::~RunAction()
{
    if (fCherenkovOut.is_open()) fCherenkovOut.close();
    if (fNeutronLifetimeOut.is_open()) fNeutronLifetimeOut.close();
    if (fPhotonsPerEventOut.is_open()) fPhotonsPerEventOut.close();
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
    
    // Всегда пишем время жизни нейтрона. Добавим допант в имя файла?
    // Для простоты, пока пишем в neutron_lifetime.txt
    // Но лучше в name передавать допант или пользователь может переименовывать файл руками.
    // Сделаем UI команду для имени файла
    fNeutronLifetimeOut.open(fNeutronLifetimeFile, std::ios::out);
    if (fNeutronLifetimeOut) {
        fNeutronLifetimeOut << "# neutron_lifetime_ns\n";
    }

    fPhotonsPerEventOut.open(fPhotonsPerEventFile, std::ios::out);
    if (fPhotonsPerEventOut) {
        fPhotonsPerEventOut << "# event_id generated_optical_photons\n";
    }
}

void RunAction::EndOfRunAction(const G4Run*) {
    G4cout << "=== Run finished ===" << G4endl;
    G4cout << "Total number of optical photons produced: " << fPhotonCount << G4endl;

    if (fCherenkovOut.is_open()) {
        fCherenkovOut.flush();
        fCherenkovOut.close();
    }
    if (fNeutronLifetimeOut.is_open()) {
        fNeutronLifetimeOut.flush();
        fNeutronLifetimeOut.close();
    }
    if (fPhotonsPerEventOut.is_open()) {
        fPhotonsPerEventOut.flush();
        fPhotonsPerEventOut.close();
    }
}

void RunAction::AddPhoton() { fPhotonCount++; }

void RunAction::WriteCherenkovPhotonEnergyEV(G4double energyEV)
{
    if (!fWriteCherenkovSpectrum) return;
    if (!fCherenkovOut.is_open()) return;
    fCherenkovOut << std::setprecision(8) << energyEV << "\n";
}

void RunAction::WriteNeutronLifetime(G4double timeNS)
{
    if (fNeutronLifetimeOut.is_open()) {
        fNeutronLifetimeOut << std::setprecision(8) << timeNS << "\n";
    }
}

void RunAction::WritePhotonsPerEvent(G4int eventID, G4int nPhotons)
{
    if (fPhotonsPerEventOut.is_open()) {
        fPhotonsPerEventOut << eventID << " " << nPhotons << "\n";
    }
}

EventAction::EventAction(RunAction* runAction)
    : fRunAction(runAction), fPhotonsPerEvent(0)
{}

EventAction::~EventAction() {}

void EventAction::BeginOfEventAction(const G4Event*) {
    fPhotonsPerEvent = 0;
}

void EventAction::EndOfEventAction(const G4Event* event) {
    if (fRunAction) {
        fRunAction->WritePhotonsPerEvent(event->GetEventID(), fPhotonsPerEvent);
    }
}

SteppingAction::SteppingAction(RunAction* runAction, EventAction* eventAction)
    : fRunAction(runAction), fEventAction(eventAction) {}

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
                    if (fEventAction) {
                        fEventAction->AddPhoton();
                    }

                    G4double E = track->GetTotalEnergy() / eV;
                    fRunAction->WriteCherenkovPhotonEnergyEV(E);
                }
            }
        }
    }
    
    // Проверка захвата/гибели первичного нейтрона
    if (track->GetDefinition() == G4Neutron::NeutronDefinition() && track->GetParentID() == 0) {
        if (track->GetTrackStatus() == fStopAndKill) {
            // Нейтрон поглощен (захват), запишем его время жизни
            G4double lifetimeNS = track->GetGlobalTime() / ns;
            fRunAction->WriteNeutronLifetime(lifetimeNS);
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

    auto eventAction = new EventAction(runAction);
    SetUserAction(eventAction);

    auto stepping = new SteppingAction(runAction, eventAction);
    SetUserAction(stepping);
}

