#include "construction.hh"
#include "G4SystemOfUnits.hh"
#include "G4NistManager.hh"
#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4RotationMatrix.hh"
#include "G4Transform3D.hh"

Detector::Detector(){}
Detector::~Detector(){}

G4VPhysicalVolume *Detector::Construct() {
    G4NistManager *nist = G4NistManager::Instance();
    
    // Материалы
    G4Material *worldMat = nist->FindOrBuildMaterial("G4_AIR");
    G4Material *steel = nist->FindOrBuildMaterial("G4_STAINLESS-STEEL");
    G4Material *pmma = nist->FindOrBuildMaterial("G4_PLEXIGLASS");
    G4Material *pureWater = nist->FindOrBuildMaterial("G4_WATER");
    
    // Вода с кадмием
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

    // Мировой объем
    G4Box *solidWorld = new G4Box("solidWorld", 2*m, 2*m, 2*m);
    G4LogicalVolume *logicWorld = new G4LogicalVolume(solidWorld, worldMat, "logicWorld");
    G4VPhysicalVolume *physWorld = new G4PVPlacement(0, G4ThreeVector(0., 0., 0.), 
                                                   logicWorld, "physWorld", 0, false, 0, true);

    // Стальной бак
    G4double steelTank_innerRadius = 630*mm;
    G4double steelTank_outerRadius = 632*mm;
    G4double steelTank_height = 1300*mm;
    G4double steelTank_halfHeight = steelTank_height/2;
    
    G4Tubs* solidSteelTank = new G4Tubs("solidSteelTank", steelTank_innerRadius, 
                                       steelTank_outerRadius, steelTank_halfHeight, 
                                       0, 360*deg);
    G4LogicalVolume* logicSteelTank = new G4LogicalVolume(solidSteelTank, steel, "logicSteelTank");
    new G4PVPlacement(0, G4ThreeVector(0., 0., 0.), logicSteelTank, "physSteelTank", 
                      logicWorld, false, 0, true);

    // Внутренний объем стального бака (чистая вода) - ИСПРАВЛЕНО: строго меньше
    G4Tubs* solidPureWater = new G4Tubs("solidPureWater", 0, steelTank_innerRadius - 0.001*mm, 
                                       steelTank_halfHeight - 0.001*mm, 0, 360*deg);
    G4LogicalVolume* logicPureWater = new G4LogicalVolume(solidPureWater, pureWater, "logicPureWater");
    new G4PVPlacement(0, G4ThreeVector(0., 0., 0.), logicPureWater, "physPureWater", 
                      logicSteelTank, false, 0, true);

    // ПММА сосуд - ИСПРАВЛЕНО: строго внутри воды
    G4double pmmaVessel_outerRadius = 599.999*mm;
    G4double pmmaVessel_innerRadius = 590*mm;
    G4double pmmaVessel_height = 699.999*mm;
    G4double pmmaVessel_halfHeight = pmmaVessel_height/2;
    
    G4Tubs* solidPMMAVessel = new G4Tubs("solidPMMAVessel", pmmaVessel_innerRadius, 
                                        pmmaVessel_outerRadius, pmmaVessel_halfHeight, 
                                        0, 360*deg);
    G4LogicalVolume* logicPMMAVessel = new G4LogicalVolume(solidPMMAVessel, pmma, "logicPMMAVessel");
    new G4PVPlacement(0, G4ThreeVector(0., 0., 0.), logicPMMAVessel, "physPMMAVessel", 
                      logicPureWater, false, 0, true);

    // Внутренний объем ПММА сосуда (вода с кадмием) - ИСПРАВЛЕНО: строго внутри ПММА
    G4Tubs* solidWaterCd = new G4Tubs("solidWaterCd", 0, pmmaVessel_innerRadius - 0.001*mm, 
                                     pmmaVessel_halfHeight - 0.001*mm, 0, 360*deg);
    G4LogicalVolume* logicWaterCd = new G4LogicalVolume(solidWaterCd, waterWithCd, "logicWaterCd");
    new G4PVPlacement(0, G4ThreeVector(0., 0., 0.), logicWaterCd, "physWaterCd", 
                      logicPMMAVessel, false, 0, true);

    // ФЭУ - ИСПРАВЛЕНО: используем существующий материал
    // Проверяем существование материала
    G4Material* pmtMaterial = nist->FindOrBuildMaterial("G4_BOROSILICATE_GLASS");
    if (!pmtMaterial) {
        // Если не найден, создаем собственный
        pmtMaterial = new G4Material("PMT_Glass", 2.5*g/cm3, 2);
        pmtMaterial->AddElement(nist->FindOrBuildElement("Si"), 0.3);
        pmtMaterial->AddElement(nist->FindOrBuildElement("O"), 0.7);
    }
    
    G4double pmtRadius = 100*mm;
    G4double pmtHeight = 200*mm;
    G4Tubs* solidPMT = new G4Tubs("solidPMT", 0, pmtRadius, pmtHeight/2, 0, 360*deg);
    G4LogicalVolume* logicPMT = new G4LogicalVolume(solidPMT, pmtMaterial, "logicPMT");

    // Размещение ФЭУ
    G4double pmtRingRadius = 500*mm;
    G4double pmtZpos = steelTank_halfHeight - pmtHeight/2;
    
    for (int i = 0; i < 12; i++) {
        G4double angle = i * 30*deg;
        G4double x = pmtRingRadius * std::cos(angle);
        G4double y = pmtRingRadius * std::sin(angle);
        
        new G4PVPlacement(0, G4ThreeVector(x, y, pmtZpos), 
                         logicPMT, "physPMT_top", logicPureWater, false, i, true);
        
        new G4PVPlacement(0, G4ThreeVector(x, y, -pmtZpos), 
                         logicPMT, "physPMT_bottom", logicPureWater, false, i+12, true);
    }

    return physWorld;
}