#include "action.hh"

ActionInitialization::ActionInitialization() {}
ActionInitialization::~ActionInitialization() {}
void ActionInitialization::Build() const {
    PrimaryGenerator *gen = new PrimaryGenerator();
    SetUserAction(gen);
}