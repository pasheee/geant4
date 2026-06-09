#pragma once

#include "G4SystemOfUnits.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VPhysicalVolume.hh"
#include "G4LogicalVolume.hh"
#include "G4Box.hh"
#include "G4PVPlacement.hh"
#include "G4NistManager.hh"
#include "G4SDManager.hh"
#include "G4Tubs.hh"
#include "G4RotationMatrix.hh"
#include "G4Transform3D.hh"
#include "G4VisAttributes.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4LogicalBorderSurface.hh"
#include "G4OpticalSurface.hh"



class G4GenericMessenger;

class Detector : public G4VUserDetectorConstruction {
    public:
        Detector();
        ~Detector() override;

        G4VPhysicalVolume *Construct() override;
        
    private:
        G4String fDopant;
        G4GenericMessenger* fMessenger;
};
