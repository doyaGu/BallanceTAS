#pragma once

#include "PlaybackTypes.h"

class TASProject;

struct OperationRequestStore {
    TASProject *requestedProject = nullptr;
    PlaybackType requestedPlaybackType = PlaybackType::None;
    bool requestedValidationRecording = false;
    bool clearProjectOnStop = false;

    void Clear() {
        requestedProject = nullptr;
        requestedPlaybackType = PlaybackType::None;
        requestedValidationRecording = false;
        clearProjectOnStop = false;
    }
};
