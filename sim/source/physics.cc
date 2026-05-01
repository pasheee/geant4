#include "physics.hh"
#include "G4DecayPhysics.hh"
#include "G4HadronElasticPhysicsHP.hh"
#include "G4HadronPhysicsFTFP_BERT_HP.hh"
#include "G4IonPhysics.hh"

PhysicsList::PhysicsList() {
    RegisterPhysics(new G4EmStandardPhysics());

    auto opticalPhysics = new G4OpticalPhysics();
    opticalPhysics->SetVerboseLevel(1);

    RegisterPhysics(opticalPhysics);

    // Адронная физика с поддержкой HP (High Precision) для нейтронов
    RegisterPhysics(new G4DecayPhysics());
    RegisterPhysics(new G4HadronElasticPhysicsHP());
    RegisterPhysics(new G4HadronPhysicsFTFP_BERT_HP());
    RegisterPhysics(new G4IonPhysics());
}

PhysicsList::~PhysicsList() {}
