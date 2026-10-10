#pragma once

#include <string>

#include "PlaybackTypes.h"

class TASProject;

struct OperationRequestStore {
    TASProject *requestedProject = nullptr;
    PlaybackType requestedPlaybackType = PlaybackType::None;
    bool requestedValidationRecording = false;
    bool clearProjectOnStop = false;
    // Set when a project is re-armed after its playback: it plays only when this level
    // loads again. 0 plays in whatever level loads next.
    int restartLevel = 0;
    // Validation asked for by this playback (tas.project.play), on top of TAS.Validation.
    bool validate = false;
    std::string validationReference; // a previous dump to compare against; empty for none
    bool validationExact = false;

    void Clear() {
        requestedProject = nullptr;
        requestedPlaybackType = PlaybackType::None;
        requestedValidationRecording = false;
        clearProjectOnStop = false;
        restartLevel = 0;
        validate = false;
        validationReference.clear();
        validationExact = false;
    }
};
