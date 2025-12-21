#ifndef ACTION_HH
#define ACTION_HH

#include "G4VUserActionInitialization.hh"
#include "G4UserRunAction.hh"
#include "G4UserSteppingAction.hh"
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

private:
    G4int fPhotonCount;

    // Analysis controls
    G4bool fWriteCherenkovSpectrum;
    G4String fCherenkovSpectrumFile;
    std::ofstream fCherenkovOut;
    G4GenericMessenger* fMessenger;
};

class SteppingAction : public G4UserSteppingAction {
public:
    SteppingAction(RunAction* runAction);
    ~SteppingAction();

    void UserSteppingAction(const G4Step*);

private:
    RunAction* fRunAction;
};

class ActionInitialization : public G4VUserActionInitialization {
public:
    ActionInitialization();
    ~ActionInitialization();

    void Build() const override;
};

#endif
