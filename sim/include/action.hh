#ifndef ACTION_HH
#define ACTION_HH

#include "G4VUserActionInitialization.hh"
#include "G4UserRunAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4UserEventAction.hh"
#include "globals.hh"
#include <fstream>

class G4GenericMessenger;

class RunAction : public G4UserRunAction {
public:
    RunAction();
    ~RunAction();

    void BeginOfRunAction(const G4Run*);
    void EndOfRunAction(const G4Run*);
    void AddPhoton();

    // Optional output of Cerenkov photon spectrum (for diagnostics only).
    void WriteCherenkovPhotonEnergyEV(G4double energyEV);
    G4bool IsCherenkovSpectrumWritingEnabled() const { return fWriteCherenkovSpectrum; }

    void WriteNeutronLifetime(G4double timeNS);
    void WritePhotonsPerEvent(G4int eventID, G4int nPhotons);

private:
    G4int fPhotonCount;

    // Analysis controls
    G4bool fWriteCherenkovSpectrum;
    G4String fCherenkovSpectrumFile;
    std::ofstream fCherenkovOut;
    
    G4String fNeutronLifetimeFile;
    std::ofstream fNeutronLifetimeOut;
    
    G4String fPhotonsPerEventFile;
    std::ofstream fPhotonsPerEventOut;

    G4GenericMessenger* fMessenger;
};

class EventAction : public G4UserEventAction {
public:
    EventAction(RunAction* runAction);
    ~EventAction();

    void BeginOfEventAction(const G4Event*) override;
    void EndOfEventAction(const G4Event*) override;

    void AddPhoton() { fPhotonsPerEvent++; }

private:
    RunAction* fRunAction;
    G4int fPhotonsPerEvent;
};

class SteppingAction : public G4UserSteppingAction {
public:
    SteppingAction(RunAction* runAction, EventAction* eventAction);
    ~SteppingAction();

    void UserSteppingAction(const G4Step*);

private:
    RunAction* fRunAction;
    EventAction* fEventAction;
};

class ActionInitialization : public G4VUserActionInitialization {
public:
    ActionInitialization();
    ~ActionInitialization();

    void Build() const override;
};

#endif
