#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Recorder.h"

enum class KeyTransition {
    NoChange,
    Pressed,
    Released,
    PressedAndReleased,
};

struct KeyEvent {
    size_t frame = 0;
    std::string key;
    KeyTransition transition = KeyTransition::NoChange;

    KeyEvent(size_t f, std::string k, KeyTransition t)
        : frame(f), key(std::move(k)), transition(t) {}
};

std::vector<KeyEvent> DetectScriptKeyTransitions(const RawInputState &previousState,
                                                 const RawInputState &currentState,
                                                 size_t frameIndex);
