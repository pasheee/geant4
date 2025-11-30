#include "physics.hh"

PhysicsList::PhysicsList() {
    RegisterPhysics(new G4EmStandardPhysics());

    auto opticalPhysics = new G4OpticalPhysics();
    opticalPhysics->SetVerboseLevel(1);

    RegisterPhysics(opticalPhysics);
}

PhysicsList::~PhysicsList() {}
