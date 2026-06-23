#ifndef ACTION_HH
#define ACTION_HH

#include "G4VUserActionInitialization.hh"
#include "G4UserRunAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4UserEventAction.hh"
#include "globals.hh"
#include <fstream>
#include <vector>

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

    // Neutron-capture de-excitation bookkeeping (prompt gammas + conversion e-).
    // gammaEnergiesMeV holds one entry per emitted capture gamma.
    G4bool IsCaptureGammaWritingEnabled() const { return fWriteCaptureGammas; }
    void WriteCapture(G4int Zres, G4int Ares,
                      const std::vector<G4double>& gammaEnergiesMeV,
                      G4int nConvElectrons, G4double convElectronEnergyMeV);

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

    // Neutron-capture gamma spectrum output (off by default).
    G4bool fWriteCaptureGammas;
    G4String fCaptureGammaFile;   // one row per emitted gamma:   Zres Ares Egamma_MeV
    G4String fCaptureSummaryFile; // one row per capture: Zres Ares nGamma sumEgamma nConvE sumEconvE
    std::ofstream fCaptureGammaOut;
    std::ofstream fCaptureSummaryOut;

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
