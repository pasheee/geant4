#pragma once
#include "G4SystemOfUnits.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VPhysicalVolume.hh"
#include "G4LogicalVolume.hh"
#include "G4Box.hh"
#include "G4PVPlacement.hh"
#include "G4NistManager.hh"

class Detector : public G4VUserDetectorConstruction {
    public:
        Detector();
        ~Detector();

        G4VPhysicalVolume *Construct() override;
};
