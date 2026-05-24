#include "ScriptInputTransition.h"

#include <array>

namespace {

constexpr std::array<const char *, 8> kKeyNames = {
    "up", "down", "left", "right", "lshift", "space", "q", "escape"
};

uint8_t GetKeyState(const RawInputState &state, int keyIndex) {
    switch (keyIndex) {
    case 0: return state.keyUp;
    case 1: return state.keyDown;
    case 2: return state.keyLeft;
    case 3: return state.keyRight;
    case 4: return state.keyShift;
    case 5: return state.keySpace;
    case 6: return state.keyQ;
    case 7: return state.keyEsc;
    default: return KS_IDLE;
    }
}

bool IsHeldAfterFrame(uint8_t state) {
    return (state & KS_PRESSED) != 0 && (state & KS_RELEASED) == 0;
}

} // namespace

std::vector<KeyEvent> DetectScriptKeyTransitions(const RawInputState &previousState,
                                                 const RawInputState &currentState,
                                                 size_t frameIndex) {
    std::vector<KeyEvent> events;

    for (int keyIdx = 0; keyIdx < static_cast<int>(kKeyNames.size()); ++keyIdx) {
        uint8_t prevKeyState = GetKeyState(previousState, keyIdx);
        uint8_t currentKeyState = GetKeyState(currentState, keyIdx);

        if (prevKeyState == currentKeyState) {
            continue;
        }

        bool wasPrevHeld = IsHeldAfterFrame(prevKeyState);
        bool isCurrentPressed = (currentKeyState & KS_PRESSED) != 0;
        bool isCurrentReleased = (currentKeyState & KS_RELEASED) != 0;
        bool isCurrentHeld = IsHeldAfterFrame(currentKeyState);

        KeyTransition transition = KeyTransition::NoChange;

        if (!wasPrevHeld && isCurrentPressed && isCurrentReleased) {
            transition = KeyTransition::PressedAndReleased;
        } else if (isCurrentReleased && (wasPrevHeld || isCurrentPressed)) {
            transition = KeyTransition::Released;
        } else if (!wasPrevHeld && isCurrentHeld) {
            transition = KeyTransition::Pressed;
        } else if (wasPrevHeld && !isCurrentHeld) {
            transition = KeyTransition::Released;
        }

        if (transition != KeyTransition::NoChange) {
            size_t eventFrame = frameIndex;
            if (transition == KeyTransition::Released &&
                wasPrevHeld &&
                !isCurrentHeld &&
                !isCurrentPressed &&
                !isCurrentReleased &&
                frameIndex > 0) {
                eventFrame = frameIndex - 1;
            }
            events.emplace_back(eventFrame, kKeyNames[keyIdx], transition);
        }
    }

    return events;
}
