#include "construction.hh"
#include "sensitive.hh"
#include "pmtSD.hh"
#include "G4SDManager.hh"
#include "G4PhysicalConstants.hh"
#include <cmath>



#include "G4GenericMessenger.hh"

Detector::Detector() : fDopant("Cd") {
    fMessenger = new G4GenericMessenger(this, "/det/", "Detector geometry control");
    fMessenger->DeclareProperty("dopant", fDopant, "Dopant material in central vessel (Cd, Gd, None)");
}
Detector::~Detector() {
    delete fMessenger;
}

G4VPhysicalVolume *Detector::Construct() {
    G4NistManager *nist = G4NistManager::Instance();

    auto tankColor = new G4VisAttributes(G4Color(0.6, 0.6, 0.6, 0.2));
    tankColor->SetForceWireframe(true);
    auto waterColor = new G4VisAttributes(G4Color(0.0, 0.0, 1.0, 0.1));
    waterColor->SetForceSolid(true);
    auto pmmaColor = new G4VisAttributes(G4Color(0.0, 1.0, 0.0, 0.2));
    pmmaColor->SetForceSolid(true);
    auto cdColor = new G4VisAttributes(G4Color(0.0, 1.0, 1.0, 0.3));
    cdColor->SetForceSolid(true);

    G4Material *worldMat = nist->FindOrBuildMaterial("G4_AIR");
    G4Material *steel = nist->FindOrBuildMaterial("G4_STAINLESS-STEEL");
    G4Material *pmma = nist->FindOrBuildMaterial("G4_PLEXIGLASS");
    G4Material *pureWater = nist->FindOrBuildMaterial("G4_WATER");

    G4Element* H = nist->FindOrBuildElement("H");
    G4Element* O = nist->FindOrBuildElement("O"); 
    G4Element* Cd = nist->FindOrBuildElement("Cd");

    G4Material* waterWithCd = nullptr;
    if (fDopant == "Gd" || fDopant == "Cd") {
        waterWithCd = new G4Material("WaterDoped", 1.001*g/cm3, 3);
        G4double massH = 0.1119;
        G4double massO = 0.8881;
        G4double massDopant = 0.0010;
        G4double total = massH + massO + massDopant;

        waterWithCd->AddElement(H, massH / total);
        waterWithCd->AddElement(O, massO / total);
        
        if (fDopant == "Gd") {
            G4Element* Gd = nist->FindOrBuildElement("Gd");
            waterWithCd->AddElement(Gd, massDopant / total);
        } else {
            waterWithCd->AddElement(Cd, massDopant / total);
        }
    } else {
        waterWithCd = pureWater;
    }

    G4Box *solidWorld = new G4Box("solidWorld", 2*m, 2*m, 2*m);
    G4LogicalVolume *logicWorld = new G4LogicalVolume(solidWorld, worldMat, "logicWorld");
    logicWorld->SetVisAttributes(G4VisAttributes::GetInvisible());
    G4VPhysicalVolume *physWorld = new G4PVPlacement(0, G4ThreeVector(0., 0., 0.), 
                                                   logicWorld, "physWorld", 0, false, 0, true);

    G4double steelTank_innerRadius = 630*mm;
    G4double steelTank_outerRadius = 632*mm;
    G4double steelTank_height = 1300*mm;
    G4double steelTank_halfHeight = steelTank_height/2;
    G4double steelWallThickness = steelTank_outerRadius - steelTank_innerRadius; // 2 mm

    // Solid steel "can": fully encloses the water on the sides and the top/bottom.
    // The inner water volume (placed below) is its daughter, so the only external
    // neighbour of the water is steel -> the reflective border surface can act on
    // all walls (sides + end caps).
    G4Tubs* solidSteelTank = new G4Tubs("solidSteelTank", 0, 
                                       steelTank_outerRadius, steelTank_halfHeight, 
                                       0, 360*deg);
    G4LogicalVolume* logicSteelTank = new G4LogicalVolume(solidSteelTank, steel, "logicSteelTank");
    logicSteelTank->SetVisAttributes(tankColor);
    
    G4VPhysicalVolume* physSteelTank = new G4PVPlacement(0, G4ThreeVector(0., 0., 0.),
                      logicSteelTank, "physSteelTank", logicWorld, false, 0, true);


    G4OpticalSurface* mirrorSurface = new G4OpticalSurface("MirrorSurface");
    mirrorSurface->SetType(dielectric_metal);
    mirrorSurface->SetFinish(polished);
    mirrorSurface->SetModel(unified);

    const G4int nEntriesMirror = 2;
    // Cover the full optical range used below (200–800 nm)
    G4double photonEnergyMirror[nEntriesMirror] = {(h_Planck * c_light) / (800. * nm),
                                                   (h_Planck * c_light) / (200. * nm)};
    G4double reflectivity[nEntriesMirror] = {0.9, 0.9};

    auto mirrorMPT = new G4MaterialPropertiesTable();
    mirrorMPT->AddProperty("REFLECTIVITY", photonEnergyMirror, reflectivity, nEntriesMirror);
    mirrorSurface->SetMaterialPropertiesTable(mirrorMPT);

    // Water buffer: daughter of the steel can, in direct contact with the steel on
    // all sides (no air gap). Wall/end-cap thickness = steelWallThickness.
    G4double pureWater_outerRadius = steelTank_outerRadius - steelWallThickness; // 630 mm
    G4double pureWater_halfHeight = steelTank_halfHeight - steelWallThickness;   // 648 mm

    G4Tubs* solidPureWater = new G4Tubs("solidPureWater", 0, pureWater_outerRadius, 
                                       pureWater_halfHeight, 0, 360*deg);
    G4LogicalVolume* logicPureWater = new G4LogicalVolume(solidPureWater, pureWater, "logicPureWater");
    logicPureWater->SetVisAttributes(waterColor);
    G4VPhysicalVolume* physPureWater = new G4PVPlacement(0, G4ThreeVector(0., 0., 0.),
                      logicPureWater, "physPureWater", logicSteelTank, false, 0, true);

    // Reflective tank walls: directional border surface acting only on photons going
    // from the water into the steel (outer wall + end caps). Internal water boundaries
    // with daughters (PMTs, PMMA vessel, doped target) are different volume pairs and
    // are therefore unaffected.
    new G4LogicalBorderSurface("WaterTankMirror", physPureWater, physSteelTank, mirrorSurface);

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

    // === SENSITIVE DETECTOR ATTACHMENT === //
    auto sdMan = G4SDManager::GetSDMpointer();
    auto waterSD = new SensitiveDetector("WaterCdSD");
    sdMan->AddNewDetector(waterSD);
    logicWaterCd->SetSensitiveDetector(waterSD);

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

    // === PMT SENSITIVE DETECTOR ===
    auto pmtsdMan = G4SDManager::GetSDMpointer();
    auto pmtSD = new PMTSD("PMTSD");
    pmtsdMan->AddNewDetector(pmtSD);
    logicPMT->SetSensitiveDetector(pmtSD);

    G4double pmtZpos = pureWater_halfHeight - pmtHeight/2 - 0.1*mm;
    if (pmtZpos < 0) pmtZpos = 0;

    // 1 ФЭУ в центре
    new G4PVPlacement(0, G4ThreeVector(0, 0, pmtZpos), 
                     logicPMT, "physPMT_top_center", logicPureWater, false, 0, true);
    new G4PVPlacement(0, G4ThreeVector(0, 0, -pmtZpos), 
                     logicPMT, "physPMT_bottom_center", logicPureWater, false, 19, true);

    // 6 ФЭУ на среднем кольце (радиус 250 мм)
    G4double pmtMiddleRingRadius = 250*mm;
    for (int i = 0; i < 6; i++) {
        G4double angle = i * 60*deg;
        G4double x = pmtMiddleRingRadius * std::cos(angle);
        G4double y = pmtMiddleRingRadius * std::sin(angle);
        
        new G4PVPlacement(0, G4ThreeVector(x, y, pmtZpos), 
                         logicPMT, "physPMT_top_mid", logicPureWater, false, i + 1, true);
        
        new G4PVPlacement(0, G4ThreeVector(x, y, -pmtZpos), 
                         logicPMT, "physPMT_bottom_mid", logicPureWater, false, i + 20, true);
    }

    // 12 ФЭУ на внешнем кольце (радиус 500 мм)
    G4double pmtOuterRingRadius = 500*mm;
    for (int i = 0; i < 12; i++) {
        G4double angle = i * 30*deg;
        G4double x = pmtOuterRingRadius * std::cos(angle);
        G4double y = pmtOuterRingRadius * std::sin(angle);
        
        new G4PVPlacement(0, G4ThreeVector(x, y, pmtZpos), 
                         logicPMT, "physPMT_top_out", logicPureWater, false, i + 7, true);
        
        new G4PVPlacement(0, G4ThreeVector(x, y, -pmtZpos), 
                         logicPMT, "physPMT_bottom_out", logicPureWater, false, i + 26, true);
    }


    // =====================================================================
    // Optical properties
    // =====================================================================
    //
    // 1) Water dispersion n(λ)
    // Source: Hale & Querry (1973), "Optical constants of water..."
    // Data via refractiveindex.info database (public domain, CC0).
    //
    // Note: Geant4 expects optical properties as a function of photon ENERGY
    // in strictly increasing order. We define wavelengths from long->short
    // so that energy is increasing.
    const G4int nWaterEntries = 25;
    G4double waterWavelength[nWaterEntries] = {
        800. * nm, 775. * nm, 750. * nm, 725. * nm, 700. * nm, 675. * nm, 650. * nm,
        625. * nm, 600. * nm, 575. * nm, 550. * nm, 525. * nm, 500. * nm, 475. * nm,
        450. * nm, 425. * nm, 400. * nm, 375. * nm, 350. * nm, 325. * nm, 300. * nm,
        275. * nm, 250. * nm, 225. * nm, 200. * nm};

    // n(λ) for liquid water at ~25 °C
    G4double rindexWater[nWaterEntries] = {
        1.329000, 1.330000, 1.330000, 1.330000, 1.331000, 1.331000, 1.331000, 1.332000,
        1.332000, 1.333000, 1.333000, 1.334000, 1.335000, 1.336000, 1.337000, 1.338000,
        1.339000, 1.341000, 1.343000, 1.346000, 1.349000, 1.354000, 1.362000, 1.373000,
        1.396000};

    G4double photonEnergyWater[nWaterEntries];
    for (int i = 0; i < nWaterEntries; ++i) {
        photonEnergyWater[i] = (h_Planck * c_light) / waterWavelength[i];
    }

    // 2) Absorption length L_abs(λ)
    // From the same Hale & Querry dataset: use extinction coefficient k(λ)
    // and convert to absorption length via:
    //   alpha = 4*pi*k / lambda,   L_abs = 1/alpha = lambda / (4*pi*k)
    //
    // NOTE: This is *absorption* (no scattering). Real detector attenuation can be shorter.
    G4double kWater[nWaterEntries] = {
        1.250e-07, 1.480e-07, 1.560e-07, 9.150e-08, 3.350e-08, 2.230e-08, 1.640e-08,
        1.390e-08, 1.090e-08, 3.600e-09, 1.960e-09, 1.320e-09, 1.000e-09, 9.350e-10,
        1.020e-09, 1.300e-09, 1.860e-09, 3.500e-09, 6.500e-09, 1.080e-08, 1.600e-08,
        2.350e-08, 3.350e-08, 4.900e-08, 1.100e-07};

    G4double absorption_pureWater[nWaterEntries];
    G4double absorption_waterCd[nWaterEntries];
    const G4double cdScale = (6.0 / 15.0); // keep previous relative transparency as a simple approximation
    for (int i = 0; i < nWaterEntries; ++i) {
        const G4double k = kWater[i];
        if (k > 0.) {
            absorption_pureWater[i] = waterWavelength[i] / (4.0 * pi * k);
        } else {
            absorption_pureWater[i] = 1e6 * m; // effectively transparent if k=0
        }
        absorption_waterCd[i] = absorption_pureWater[i] * cdScale;
    }

    auto waterMPT = new G4MaterialPropertiesTable();
    waterMPT->AddProperty("RINDEX", photonEnergyWater, rindexWater, nWaterEntries);
    waterMPT->AddProperty("ABSLENGTH", photonEnergyWater, absorption_pureWater, nWaterEntries);
    pureWater->SetMaterialPropertiesTable(waterMPT);

    // PMMA (acrylic) vessel optical properties. Without RINDEX optical photons are
    // killed at the water<->PMMA boundary (NoRINDEX). Treat the thin acrylic wall as
    // transparent: constant n~1.49, large absorption length.
    G4double rindexPMMA[nWaterEntries];
    G4double absorption_pmma[nWaterEntries];
    for (int i = 0; i < nWaterEntries; ++i) {
        rindexPMMA[i] = 1.49;
        absorption_pmma[i] = 10.0 * m;
    }
    auto pmmaMPT = new G4MaterialPropertiesTable();
    pmmaMPT->AddProperty("RINDEX", photonEnergyWater, rindexPMMA, nWaterEntries);
    pmmaMPT->AddProperty("ABSLENGTH", photonEnergyWater, absorption_pmma, nWaterEntries);
    pmma->SetMaterialPropertiesTable(pmmaMPT);

    auto waterCdMPT = new G4MaterialPropertiesTable();
    waterCdMPT->AddProperty("RINDEX", photonEnergyWater, rindexWater, nWaterEntries);
    waterCdMPT->AddProperty("ABSLENGTH", photonEnergyWater, absorption_waterCd, nWaterEntries);
    waterWithCd->SetMaterialPropertiesTable(waterCdMPT);

    auto glassMPT = new G4MaterialPropertiesTable();

    // PMT window (borosilicate glass) refractive index: keep constant but defined over full range.
    G4double rindexGlass[nWaterEntries];
    for (int i = 0; i < nWaterEntries; ++i) rindexGlass[i] = 1.52;
    glassMPT->AddProperty("RINDEX", photonEnergyWater, rindexGlass, nWaterEntries);

    // 3) PMT quantum efficiency QE(λ) -> used by PMTSD as "EFFICIENCY"
    // Source: WCSim (MIT) QE curve (10-inch HQE PMT), tabulated vs wavelength.
    // https://github.com/WCSim/WCSim  (see src/WCSimPMTObject.cc, PMT10inchHQE::GetQE)
    const G4int nQE = 20;
    G4double qeWavelength[nQE] = {660. * nm, 640. * nm, 620. * nm, 600. * nm, 580. * nm, 560. * nm,
                                  540. * nm, 520. * nm, 500. * nm, 480. * nm, 460. * nm, 440. * nm,
                                  420. * nm, 400. * nm, 380. * nm, 360. * nm, 340. * nm, 320. * nm,
                                  300. * nm, 280. * nm};
    // QE in fraction (0..1), reversed order to match qeWavelength (660->280 nm)
    G4double quantumEfficiency[nQE] = {0.00, 0.0061, 0.0178, 0.0323, 0.0499, 0.0727, 0.1102, 0.1641,
                                       0.1971, 0.2268, 0.2655, 0.2915, 0.3168, 0.3320, 0.3396, 0.3306,
                                       0.2933, 0.2017, 0.0502, 0.00};
    G4double photonEnergyQE[nQE];
    for (int i = 0; i < nQE; ++i) {
        photonEnergyQE[i] = (h_Planck * c_light) / qeWavelength[i];
    }
    glassMPT->AddProperty("EFFICIENCY", photonEnergyQE, quantumEfficiency, nQE);
    
    pmtMaterial->SetMaterialPropertiesTable(glassMPT);
    
    return physWorld;
}
