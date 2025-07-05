#include "InputSystem.h"

#include <algorithm>
#include <cctype>
#include <sstream>

// Helper function to convert a string to lowercase
static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Helper function to trim whitespace
static std::string Trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";

    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

InputSystem::InputSystem() {
    InitializeKeyMap();
}

std::vector<std::string> InputSystem::ParseKeyString(const std::string &keyString) const {
    std::vector<std::string> keys;
    std::string current;

    for (char ch : keyString) {
        if (isspace(ch)) {
            if (!current.empty()) {
                std::string trimmed = Trim(current);
                if (!trimmed.empty()) {
                    keys.push_back(ToLower(trimmed));
                }
                current.clear();
            }
        } else {
            current += ch;
        }
    }

    // Add the last key
    if (!current.empty()) {
        std::string trimmed = Trim(current);
        if (!trimmed.empty()) {
            keys.push_back(ToLower(trimmed));
        }
    }

    // Remove duplicates while preserving order
    std::vector<std::string> uniqueKeys;
    std::set<std::string> seen;
    for (const auto& key : keys) {
        if (seen.find(key) == seen.end()) {
            uniqueKeys.push_back(key);
            seen.insert(key);
        }
    }

    return uniqueKeys;
}

void InputSystem::InitializeKeyMap() {
    // Clear any existing mappings
    m_Keymap.clear();

    // Arrow keys
    m_Keymap["up"] = CKKEY_UP;
    m_Keymap["down"] = CKKEY_DOWN;
    m_Keymap["left"] = CKKEY_LEFT;
    m_Keymap["right"] = CKKEY_RIGHT;

    // Modifier keys
    m_Keymap["lshift"] = CKKEY_LSHIFT;
    m_Keymap["rshift"] = CKKEY_RSHIFT;
    m_Keymap["shift"] = CKKEY_LSHIFT; // Alias for left shift
    m_Keymap["lctrl"] = CKKEY_LCONTROL;
    m_Keymap["rctrl"] = CKKEY_RCONTROL;
    m_Keymap["ctrl"] = CKKEY_LCONTROL; // Alias for left ctrl
    m_Keymap["lalt"] = CKKEY_LMENU;
    m_Keymap["ralt"] = CKKEY_RMENU;
    m_Keymap["alt"] = CKKEY_LMENU; // Alias for left alt

    // Special keys
    m_Keymap["space"] = CKKEY_SPACE;
    m_Keymap["enter"] = CKKEY_RETURN;
    m_Keymap["return"] = CKKEY_RETURN;
    m_Keymap["esc"] = CKKEY_ESCAPE;
    m_Keymap["escape"] = CKKEY_ESCAPE;
    m_Keymap["tab"] = CKKEY_TAB;
    m_Keymap["backspace"] = CKKEY_BACK;
    m_Keymap["delete"] = CKKEY_DELETE;
    m_Keymap["insert"] = CKKEY_INSERT;
    m_Keymap["home"] = CKKEY_HOME;
    m_Keymap["end"] = CKKEY_END;
    m_Keymap["pageup"] = CKKEY_PRIOR;
    m_Keymap["pagedown"] = CKKEY_NEXT;

    // Numbers (individual mapping according to CKKEYBOARD enum)
    m_Keymap["1"] = CKKEY_1;
    m_Keymap["2"] = CKKEY_2;
    m_Keymap["3"] = CKKEY_3;
    m_Keymap["4"] = CKKEY_4;
    m_Keymap["5"] = CKKEY_5;
    m_Keymap["6"] = CKKEY_6;
    m_Keymap["7"] = CKKEY_7;
    m_Keymap["8"] = CKKEY_8;
    m_Keymap["9"] = CKKEY_9;
    m_Keymap["0"] = CKKEY_0;

    // Letters (mapped according to QWERTY layout order in CKKEYBOARD enum)
    // Top row: Q W E R T Y U I O P
    m_Keymap["q"] = CKKEY_Q;
    m_Keymap["w"] = CKKEY_W;
    m_Keymap["e"] = CKKEY_E;
    m_Keymap["r"] = CKKEY_R;
    m_Keymap["t"] = CKKEY_T;
    m_Keymap["y"] = CKKEY_Y;
    m_Keymap["u"] = CKKEY_U;
    m_Keymap["i"] = CKKEY_I;
    m_Keymap["o"] = CKKEY_O;
    m_Keymap["p"] = CKKEY_P;

    // Middle row: A S D F G H J K L
    m_Keymap["a"] = CKKEY_A;
    m_Keymap["s"] = CKKEY_S;
    m_Keymap["d"] = CKKEY_D;
    m_Keymap["f"] = CKKEY_F;
    m_Keymap["g"] = CKKEY_G;
    m_Keymap["h"] = CKKEY_H;
    m_Keymap["j"] = CKKEY_J;
    m_Keymap["k"] = CKKEY_K;
    m_Keymap["l"] = CKKEY_L;

    // Bottom row: Z X C V B N M
    m_Keymap["z"] = CKKEY_Z;
    m_Keymap["x"] = CKKEY_X;
    m_Keymap["c"] = CKKEY_C;
    m_Keymap["v"] = CKKEY_V;
    m_Keymap["b"] = CKKEY_B;
    m_Keymap["n"] = CKKEY_N;
    m_Keymap["m"] = CKKEY_M;

    // Function keys (individual mapping - F11 and F12 are not sequential!)
    m_Keymap["f1"] = CKKEY_F1;
    m_Keymap["f2"] = CKKEY_F2;
    m_Keymap["f3"] = CKKEY_F3;
    m_Keymap["f4"] = CKKEY_F4;
    m_Keymap["f5"] = CKKEY_F5;
    m_Keymap["f6"] = CKKEY_F6;
    m_Keymap["f7"] = CKKEY_F7;
    m_Keymap["f8"] = CKKEY_F8;
    m_Keymap["f9"] = CKKEY_F9;
    m_Keymap["f10"] = CKKEY_F10;
    m_Keymap["f11"] = CKKEY_F11; // 0x57 - NOT sequential!
    m_Keymap["f12"] = CKKEY_F12; // 0x58 - NOT sequential!
    m_Keymap["f13"] = CKKEY_F13;
    m_Keymap["f14"] = CKKEY_F14;
    m_Keymap["f15"] = CKKEY_F15;

    // Numpad keys
    m_Keymap["numpad0"] = CKKEY_NUMPAD0;
    m_Keymap["numpad1"] = CKKEY_NUMPAD1;
    m_Keymap["numpad2"] = CKKEY_NUMPAD2;
    m_Keymap["numpad3"] = CKKEY_NUMPAD3;
    m_Keymap["numpad4"] = CKKEY_NUMPAD4;
    m_Keymap["numpad5"] = CKKEY_NUMPAD5;
    m_Keymap["numpad6"] = CKKEY_NUMPAD6;
    m_Keymap["numpad7"] = CKKEY_NUMPAD7;
    m_Keymap["numpad8"] = CKKEY_NUMPAD8;
    m_Keymap["numpad9"] = CKKEY_NUMPAD9;
    m_Keymap["multiply"] = CKKEY_MULTIPLY;
    m_Keymap["add"] = CKKEY_ADD;
    m_Keymap["subtract"] = CKKEY_SUBTRACT;
    m_Keymap["decimal"] = CKKEY_DECIMAL;
    m_Keymap["divide"] = CKKEY_DIVIDE;

    // Common punctuation
    m_Keymap[";"] = CKKEY_SEMICOLON;
    m_Keymap["="] = CKKEY_EQUALS;
    m_Keymap[","] = CKKEY_COMMA;
    m_Keymap["-"] = CKKEY_MINUS;
    m_Keymap["."] = CKKEY_PERIOD;
    m_Keymap["/"] = CKKEY_SLASH;
    m_Keymap["`"] = CKKEY_GRAVE;
    m_Keymap["["] = CKKEY_LBRACKET;
    m_Keymap["\\"] = CKKEY_BACKSLASH;
    m_Keymap["]"] = CKKEY_RBRACKET;
    m_Keymap["'"] = CKKEY_APOSTROPHE;

    // Additional useful aliases
    m_Keymap["capslock"] = CKKEY_CAPITAL;
    m_Keymap["numlock"] = CKKEY_NUMLOCK;
    m_Keymap["scrolllock"] = CKKEY_SCROLL;
    m_Keymap["lwin"] = CKKEY_LWIN;
    m_Keymap["rwin"] = CKKEY_RWIN;
    m_Keymap["menu"] = CKKEY_APPS;
}

CKKEYBOARD InputSystem::GetKeyCode(const std::string &key) const {
    if (key.empty()) {
        return static_cast<CKKEYBOARD>(0);
    }

    auto it = m_Keymap.find(ToLower(key));
    if (it != m_Keymap.end()) {
        return it->second;
    }
    return static_cast<CKKEYBOARD>(0);
}

bool InputSystem::IsValidKeyCode(CKKEYBOARD keyCode) {
    // Check if the key code is within the valid range for keyboard state array
    // Most keyboard state arrays are 256 bytes, and all CKKEYBOARD values should fit
    return keyCode > 0 && keyCode < 256;
}

bool InputSystem::IsValidKey(const std::string &key) const {
    return GetKeyCode(key) != 0;
}

void InputSystem::Press(const std::string &keyString) {
    auto keys = ParseKeyString(keyString);
    for (const auto& key : keys) {
        CKKEYBOARD code = GetKeyCode(key);
        if (IsValidKeyCode(code)) {
            m_PressedKeys.insert(code);
        }
    }
}

void InputSystem::Hold(const std::string &keyString, int durationTicks) {
    if (durationTicks <= 0) {
        return; // Invalid duration
    }

    auto keys = ParseKeyString(keyString);
    for (const auto& key : keys) {
        CKKEYBOARD code = GetKeyCode(key);
        if (IsValidKeyCode(code)) {
            m_HeldKeys[code] = {durationTicks};
        }
    }
}

void InputSystem::KeyDown(const std::string &keyString) {
    auto keys = ParseKeyString(keyString);
    for (const auto& key : keys) {
        CKKEYBOARD code = GetKeyCode(key);
        if (IsValidKeyCode(code)) {
            m_DownKeys.insert(code);
        }
    }
}

void InputSystem::KeyUp(const std::string &keyString) {
    auto keys = ParseKeyString(keyString);
    for (const auto& key : keys) {
        CKKEYBOARD code = GetKeyCode(key);
        if (IsValidKeyCode(code)) {
            m_DownKeys.erase(code);
        }
    }
}

void InputSystem::ReleaseAllKeys() {
    m_PressedKeys.clear();
    m_HeldKeys.clear();
    m_DownKeys.clear();
}

void InputSystem::Reset() {
    ReleaseAllKeys();
    m_PreviouslyControlledKeys.clear();
}

bool InputSystem::IsKeyPressed(const std::string &keyString) const {
    auto keys = ParseKeyString(keyString);
    if (keys.empty()) return false;

    // For combinations, ALL keys must be pressed
    for (const auto& key : keys) {
        CKKEYBOARD code = GetKeyCode(key);
        if (!IsValidKeyCode(code)) return false;

        bool isPressed = m_PressedKeys.count(code) > 0 ||
                        m_HeldKeys.count(code) > 0 ||
                        m_DownKeys.count(code) > 0;

        if (!isPressed) return false;
    }

    return true;
}

std::vector<std::string> InputSystem::GetAvailableKeys() const {
    std::vector<std::string> keys;
    keys.reserve(m_Keymap.size());

    for (const auto &pair : m_Keymap) {
        keys.push_back(pair.first);
    }

    std::sort(keys.begin(), keys.end());
    return keys;
}

void InputSystem::Apply(unsigned char *keyboardState) {
    if (!keyboardState) {
        return;
    }

    // --- Phase 1: Apply keys BEFORE decrementing counters ---
    // This ensures keys are held for the correct duration

    std::set<CKKEYBOARD> controlledKeys;

    // Apply one-frame presses
    for (CKKEYBOARD key : m_PressedKeys) {
        if (IsValidKeyCode(key)) {
            keyboardState[key] = KS_PRESSED;
            controlledKeys.insert(key);
        }
    }

    // Apply multi-frame holds
    for (const auto &pair : m_HeldKeys) {
        CKKEYBOARD key = pair.first;
        if (IsValidKeyCode(key)) {
            keyboardState[key] = KS_PRESSED;
            controlledKeys.insert(key);
        }
    }

    // Apply manually managed down keys
    for (CKKEYBOARD key : m_DownKeys) {
        if (IsValidKeyCode(key)) {
            keyboardState[key] = KS_PRESSED;
            controlledKeys.insert(key);
        }
    }

    // --- Phase 2: Update internal state AFTER applying ---

    // NOW decrement ticks for held keys
    for (auto it = m_HeldKeys.begin(); it != m_HeldKeys.end();) {
        it->second.remainingTicks--;
        if (it->second.remainingTicks <= 0) {
            it = m_HeldKeys.erase(it);
        } else {
            ++it;
        }
    }

    // Release previously controlled keys that aren't controlled this frame
    for (const auto &prev_key : m_PreviouslyControlledKeys) {
        if (controlledKeys.find(prev_key) == controlledKeys.end()) {
            if (IsValidKeyCode(prev_key)) {
                keyboardState[prev_key] = KS_RELEASED;
            }
        }
    }

    m_PreviouslyControlledKeys = controlledKeys;

    // Clear one-frame presses
    m_PressedKeys.clear();
}