#include "action.hh"
#include "generator.hh"
#include "G4RunManager.hh"
#include "G4Event.hh"
#include "G4Step.hh"
#include "G4OpticalPhoton.hh"
#include "G4Track.hh"
#include "G4SystemOfUnits.hh"

RunAction::RunAction() : fPhotonCount(0) {}
RunAction::~RunAction() {}

void RunAction::BeginOfRunAction(const G4Run*) {
    fPhotonCount = 0;
    G4cout << "=== Run started ===" << G4endl;
}

void RunAction::EndOfRunAction(const G4Run*) {
    G4cout << "=== Run finished ===" << G4endl;
    G4cout << "Total number of optical photons produced: " << fPhotonCount << G4endl;
}

void RunAction::AddPhoton() { fPhotonCount++; }

SteppingAction::SteppingAction(RunAction* runAction) : fRunAction(runAction) {}
SteppingAction::~SteppingAction() {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
    auto track = step->GetTrack();
    if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
        fRunAction->AddPhoton();
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
