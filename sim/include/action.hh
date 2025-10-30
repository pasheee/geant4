#ifndef ACTION_HH
#define ACTION_HH

#include "G4VUserActionInitialization.hh"
#include "G4UserRunAction.hh"
#include "G4UserSteppingAction.hh"
#include "globals.hh"

class RunAction : public G4UserRunAction {
public:
    RunAction();
    ~RunAction();

    void BeginOfRunAction(const G4Run*);
    void EndOfRunAction(const G4Run*);
    void AddPhoton();

private:
    G4int fPhotonCount;
};

class SteppingAction : public G4UserSteppingAction {
public:
    SteppingAction(RunAction* runAction);
    ~SteppingAction();

    void UserSteppingAction(const G4Step*);

private:
    RunAction* fRunAction;
};

class ActionInitialization : public G4VUserActionInitialization {
public:
    ActionInitialization();
    ~ActionInitialization();

    void Build() const override;
};

#endif
