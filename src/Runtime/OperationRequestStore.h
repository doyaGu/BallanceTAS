#pragma once

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

    void Clear() {
        requestedProject = nullptr;
        requestedPlaybackType = PlaybackType::None;
        requestedValidationRecording = false;
        clearProjectOnStop = false;
        restartLevel = 0;
    }
};
