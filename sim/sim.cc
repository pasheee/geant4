#include "G4RunManager.hh"
#include "G4UImanager.hh"
#include "G4VisManager.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"
#include "Randomize.hh"
#include "construction.hh"
#include "physics.hh"
#include "action.hh"
#include <chrono>
#include <cstdlib>

int main(int argc, char** argv) {
    long seed = 0;
    if (const char* env = std::getenv("SIM_SEED")) {
        char* end = nullptr;
        const long parsed = std::strtol(env, &end, 10);
        if (end != env) seed = parsed;
    }
    if (seed == 0) {
        const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        seed = static_cast<long>(now);
    }
    G4Random::setTheSeed(seed);
    G4cout << "=== Random seed: " << seed << " ===" << G4endl;

    G4RunManager *runManager = new G4RunManager();
    runManager->SetUserInitialization(new Detector());
    runManager->SetUserInitialization(new PhysicsList());
    runManager->SetUserInitialization(new ActionInitialization());

    G4UImanager *UImanager = G4UImanager::GetUIpointer();

    if (argc == 1) {
        G4UIExecutive *ui = new G4UIExecutive(argc, argv);

        G4VisManager *visManager = new G4VisExecutive();
        visManager->Initialize();

        UImanager->ApplyCommand("/control/execute init_vis.mac");

        ui->SessionStart();

        delete visManager;
        delete ui;
    } else {
        G4String command = "/control/execute ";
        G4String fileName = argv[1];
        UImanager->ApplyCommand(command + fileName);
    }

    delete runManager;
    return 0;
}