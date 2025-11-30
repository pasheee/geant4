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
    // Суммарная энергия по всем PMT
    G4double fEnergySum;

    // Суммарное число фотонов по всем PMT
    G4int fPhotonCount;

    // Для пометки уже учтённых треков (чтобы не считать трек много раз)
    std::set<G4int> fSeenTracks;

    // Опционально: статистика по каждой PMT (copy number -> значение)
    std::map<G4int, G4int> fPhotonsPerPMT;
    std::map<G4int, G4double> fEnergyPerPMT;
};

#endif
