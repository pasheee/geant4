#pragma once

#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
#include "G4SystemOfUnits.hh"
#include "G4ParticleTable.hh"

class PrimaryGenerator : public G4VUserPrimaryGeneratorAction {
    public:
        PrimaryGenerator();
        ~PrimaryGenerator();

        void GeneratePrimaries(G4Event*) override;
    
    private:
        G4ParticleGun *fParticleGun;
};