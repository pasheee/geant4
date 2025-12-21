#ifndef PMTSD_HH
#define PMTSD_HH

#include "G4VSensitiveDetector.hh"
#include "G4Step.hh"
#include "G4TouchableHistory.hh"
#include "G4SystemOfUnits.hh"
#include <set>
#include <map>

class PMTSD : public G4VSensitiveDetector {
public:
    PMTSD(const G4String& name);
    virtual ~PMTSD();

    virtual void Initialize(G4HCofThisEvent*) override;
    virtual G4bool ProcessHits(G4Step* aStep, G4TouchableHistory* ROhist) override;
    virtual void EndOfEvent(G4HCofThisEvent*) override;

private:
    G4double fEnergySum;

    G4int fPhotoElectronCount;

    G4int fPhotoElectronCountTop;
    G4int fPhotoElectronCountBottom;

    std::set<G4int> fSeenTracks;

    std::map<G4int, G4int> fPhotoElectronsPerPMT;
    std::map<G4int, G4double> fEnergyPerPMT;
};

#endif
