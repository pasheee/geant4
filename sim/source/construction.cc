#include "construction.hh"
#include "G4SystemOfUnits.hh"
#include "G4NistManager.hh"
#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4RotationMatrix.hh"
#include "G4Transform3D.hh"
#include "G4VisAttributes.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4OpticalSurface.hh"

Detector::Detector(){}
Detector::~Detector(){}

G4VPhysicalVolume *Detector::Construct() {
    G4NistManager *nist = G4NistManager::Instance();

    auto tankColor = new G4VisAttributes(G4Color(255, 0, 0, 1));
    tankColor -> SetForceSolid(true);
    auto waterColor = new G4VisAttributes(G4Color(0, 0, 100, 0.5));
    waterColor -> SetForceSolid(true);
    auto pmmaColor = new G4VisAttributes(G4Color(0, 255, 0, 1.));
    pmmaColor->SetForceSolid(true);
    auto cdColor = new G4VisAttributes(G4Color(0, 255, 255, 0.3));
    cdColor->SetForceSolid(true);

    G4Material *worldMat = nist->FindOrBuildMaterial("G4_AIR");
    G4Material *steel = nist->FindOrBuildMaterial("G4_STAINLESS-STEEL");
    G4Material *pmma = nist->FindOrBuildMaterial("G4_PLEXIGLASS");
    G4Material *pureWater = nist->FindOrBuildMaterial("G4_WATER");

    G4Element* H = nist->FindOrBuildElement("H");
    G4Element* O = nist->FindOrBuildElement("O"); 
    G4Element* Cd = nist->FindOrBuildElement("Cd");

    G4Material* waterWithCd = new G4Material("WaterWithCd", 1.001*g/cm3, 3);
    G4double massH = 0.1119;
    G4double massO = 0.8881;
    G4double massCd = 0.0010;
    G4double total = massH + massO + massCd;

    waterWithCd->AddElement(H, massH / total);
    waterWithCd->AddElement(O, massO / total);
    waterWithCd->AddElement(Cd, massCd / total);

    G4Box *solidWorld = new G4Box("solidWorld", 2*m, 2*m, 2*m);
    G4LogicalVolume *logicWorld = new G4LogicalVolume(solidWorld, worldMat, "logicWorld");
    G4VPhysicalVolume *physWorld = new G4PVPlacement(0, G4ThreeVector(0., 0., 0.), 
                                                   logicWorld, "physWorld", 0, false, 0, true);

    G4double steelTank_innerRadius = 630*mm;
    G4double steelTank_outerRadius = 632*mm;
    G4double steelTank_height = 1300*mm;
    G4double steelTank_halfHeight = steelTank_height/2;

    G4Tubs* solidSteelTank = new G4Tubs("solidSteelTank", steelTank_innerRadius, 
                                       steelTank_outerRadius, steelTank_halfHeight, 
                                       0, 360*deg);
    G4LogicalVolume* logicSteelTank = new G4LogicalVolume(solidSteelTank, steel, "logicSteelTank");
    logicSteelTank->SetVisAttributes(tankColor);
    
    new G4PVPlacement(0, G4ThreeVector(0., 0., 0.), logicSteelTank, "physSteelTank", 
                      logicWorld, false, 0, true);


    G4OpticalSurface* mirrorSurface = new G4OpticalSurface("MirrorSurface");
    mirrorSurface->SetType(dielectric_metal);
    mirrorSurface->SetFinish(polished);
    mirrorSurface->SetModel(unified);

    const G4int nEntriesMirror = 2;
    G4double photonEnergyMirror[nEntriesMirror] = {2.0*eV, 7.0*eV};
    G4double reflectivity[nEntriesMirror] = {0.9, 0.9};

    auto mirrorMPT = new G4MaterialPropertiesTable();
    mirrorMPT->AddProperty("REFLECTIVITY", photonEnergyMirror, reflectivity, nEntriesMirror);
    mirrorSurface->SetMaterialPropertiesTable(mirrorMPT);

    new G4LogicalSkinSurface("SteelTankMirror", logicSteelTank, mirrorSurface);

    G4double pureWater_outerRadius = steelTank_innerRadius - 0.001*mm;
    G4double pureWater_halfHeight = steelTank_halfHeight - 0.001*mm;

    G4Tubs* solidPureWater = new G4Tubs("solidPureWater", 0, pureWater_outerRadius, 
                                       pureWater_halfHeight, 0, 360*deg);
    G4LogicalVolume* logicPureWater = new G4LogicalVolume(solidPureWater, pureWater, "logicPureWater");
    logicPureWater->SetVisAttributes(waterColor);
    new G4PVPlacement(0, G4ThreeVector(0., 0., 0.), logicPureWater, "physPureWater", 
                      logicWorld, false, 0, true);

    G4double pmmaVessel_outerRadius = 599.999*mm;
    G4double pmmaVessel_innerRadius = 590*mm;
    G4double pmmaVessel_height = 699.999*mm;
    G4double pmmaVessel_halfHeight = pmmaVessel_height/2;

    G4Tubs* solidPMMAVessel = new G4Tubs("solidPMMAVessel", pmmaVessel_innerRadius, 
                                        pmmaVessel_outerRadius, pmmaVessel_halfHeight, 
                                        0, 360*deg);
    G4LogicalVolume* logicPMMAVessel = new G4LogicalVolume(solidPMMAVessel, pmma, "logicPMMAVessel");
    logicPMMAVessel->SetVisAttributes(pmmaColor);
    new G4PVPlacement(0, G4ThreeVector(0., 0., 0.), logicPMMAVessel, "physPMMAVessel", 
                      logicPureWater, false, 0, true);

    G4Tubs* solidWaterCd = new G4Tubs("solidWaterCd", 0, pmmaVessel_innerRadius - 0.001*mm, 
                                     pmmaVessel_halfHeight - 0.001*mm, 0, 360*deg);
    G4LogicalVolume* logicWaterCd = new G4LogicalVolume(solidWaterCd, waterWithCd, "logicWaterCd");
    logicWaterCd->SetVisAttributes(cdColor);
    new G4PVPlacement(0, G4ThreeVector(0., 0., 0.), logicWaterCd, "physWaterCd", 
                      logicPureWater, false, 0, true);

    G4Material* pmtMaterial = nist->FindOrBuildMaterial("G4_BOROSILICATE_GLASS");
    if (!pmtMaterial) {
        pmtMaterial = new G4Material("PMT_Glass", 2.5*g/cm3, 2);
        pmtMaterial->AddElement(nist->FindOrBuildElement("Si"), 0.3);
        pmtMaterial->AddElement(nist->FindOrBuildElement("O"), 0.7);
    }

    G4double pmtRadius = 100*mm;
    G4double pmtHeight = 200*mm;
    G4Tubs* solidPMT = new G4Tubs("solidPMT", 0, pmtRadius, pmtHeight/2, 0, 360*deg);
    G4LogicalVolume* logicPMT = new G4LogicalVolume(solidPMT, pmtMaterial, "logicPMT");

    G4double pmtRingRadius = 500*mm;
    G4double pmtZpos = pureWater_halfHeight - pmtHeight/2 - 0.1*mm;
    if (pmtZpos < 0) pmtZpos = 0;

    for (int i = 0; i < 12; i++) {
        G4double angle = i * 30*deg;
        G4double x = pmtRingRadius * std::cos(angle);
        G4double y = pmtRingRadius * std::sin(angle);
        
        new G4PVPlacement(0, G4ThreeVector(x, y, pmtZpos), 
                         logicPMT, "physPMT_top", logicPureWater, false, i, true);
        
        new G4PVPlacement(0, G4ThreeVector(x, y, -pmtZpos), 
                         logicPMT, "physPMT_bottom", logicPureWater, false, i+12, true);
    }


    const G4int nEntries = 2;
    G4double photonEnergy[nEntries] = {2.0*eV, 7.0*eV};

    G4double refractiveIndex[nEntries] = {1.33, 1.33};

    G4double absorption_pureWater[nEntries] = {15.*m, 15.*m};
    G4double absorption_waterCd[nEntries] = {6.*m, 6.*m};

    auto waterMPT = new G4MaterialPropertiesTable();
    waterMPT->AddProperty("RINDEX", photonEnergy, refractiveIndex, nEntries);
    waterMPT->AddProperty("ABSLENGTH", photonEnergy, absorption_pureWater, nEntries);
    pureWater->SetMaterialPropertiesTable(waterMPT);

    auto waterCdMPT = new G4MaterialPropertiesTable();
    waterCdMPT->AddProperty("RINDEX", photonEnergy, refractiveIndex, nEntries);
    waterCdMPT->AddProperty("ABSLENGTH", photonEnergy, absorption_waterCd, nEntries);
    waterWithCd->SetMaterialPropertiesTable(waterCdMPT);

    G4double refractiveIndexGlass[nEntries] = {1.52, 1.52};
    auto glassMPT = new G4MaterialPropertiesTable();
    glassMPT->AddProperty("RINDEX", photonEnergy, refractiveIndexGlass, nEntries);
    pmtMaterial->SetMaterialPropertiesTable(glassMPT);

    G4double quantumEfficiency[nEntries] = {0.28, 0.28};
    G4MaterialPropertiesTable* pmtMPT = new G4MaterialPropertiesTable();
    pmtMPT->AddProperty("EFFICIENCY", photonEnergy, quantumEfficiency, nEntries);
    pmtMaterial->SetMaterialPropertiesTable(pmtMPT);
    
    return physWorld;
}
