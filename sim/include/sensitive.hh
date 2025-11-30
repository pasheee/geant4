#pragma once
#include "G4VSensitiveDetector.hh"
#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"
#include "G4Step.hh"
#include "G4HCofThisEvent.hh"
#include "G4TouchableHistory.hh"
#include <set>

class SensitiveDetector : public G4VSensitiveDetector {
public:
    SensitiveDetector(const G4String& name);
    ~SensitiveDetector() override;
private:
    G4double fTotalEnergyDeposed;
    G4int fPhotonCount;
    std::set<G4int> fSeenTracks;

    virtual void Initialize(G4HCofThisEvent*) override;
    virtual void EndOfEvent(G4HCofThisEvent*) override;
    virtual G4bool ProcessHits(G4Step* aStep, G4TouchableHistory* ROhis) override;
};