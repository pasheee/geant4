#pragma once

#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
#include "G4SystemOfUnits.hh"
#include "G4ParticleTable.hh"
#include "G4ThreeVector.hh"
#include "globals.hh"

class G4GenericMessenger;

class PrimaryGenerator : public G4VUserPrimaryGeneratorAction {
    public:
        PrimaryGenerator();
        ~PrimaryGenerator() override;

        void GeneratePrimaries(G4Event*) override;

    private:
        G4String fMode;        
        G4String fParticleName;       
        G4double fMonoKineticEnergy;    
        G4ThreeVector fPosition;        
        G4bool fIsotropicDirection;     
        G4ThreeVector fDirection;
        G4bool fRandomizePos;       

        G4double fNuEmin;               
        G4double fNuEmax;
        G4double fFrac235U;
        G4double fFrac238U;
        G4double fFrac239Pu;
        G4double fFrac241Pu;

        G4ParticleGun* fParticleGun;
        G4GenericMessenger* fMessenger;

        G4double fIbdWmax;
        G4bool fIbdWmaxValid;
        G4double fCachedNuEmin;
        G4double fCachedNuEmax;
        G4double fCachedFrac235U;
        G4double fCachedFrac238U;
        G4double fCachedFrac239Pu;
        G4double fCachedFrac241Pu;

        void SetupMessenger();

        void NormalizeFissionFractions();
        G4double ReactorFluxPerFission(G4double Enu) const;
        G4double IbdWeight(G4double Enu) const;
        void RecomputeIbdWmax();
        G4double SampleIbdNuEnergy();
};