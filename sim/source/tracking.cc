// #include "tracking.hh"
// #include "G4OpticalPhoton.hh"
// #include "G4Track.hh"
// #include "G4String.hh"
// #include "action.hh"

// void TrackingAction::PreUserTrackingAction(const G4Track* track) {
//     if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
//         G4String creator = track->GetCreatorProcessName();
//         if (!creator.empty() && creator.contains("Cerenkov")) {
//             fRunAction->AddPhoton();
//         }
//     }
// }
