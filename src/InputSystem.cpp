#include "InputSystem.h"

#include <set>
#include <algorithm>
#include <sstream>

// Helper function to convert a string to lowercase
static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Helper function to trim whitespace
static std::string Trim(const std::string &str) {
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
    for (const auto &key : keys) {
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

    // Numbers
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

    // Letters (QWERTY layout)
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

    // Function keys
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
    m_Keymap["f11"] = CKKEY_F11;
    m_Keymap["f12"] = CKKEY_F12;
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
    // Allow all key codes in the valid range, including 0
    return keyCode < 256;
}

bool InputSystem::IsValidKey(const std::string &key) const {
    return GetKeyCode(key) != 0;
}

void InputSystem::PressKeys(const std::string &keyString) {
    auto keys = ParseKeyString(keyString);
    for (const auto &key : keys) {
        CKKEYBOARD code = GetKeyCode(key);
        if (IsValidKeyCode(code) && code != 0) {
            // Generate a press event for this frame
            m_KeyStates[code].ApplyPressEvent(m_CurrentTick);
        }
    }
}

void InputSystem::PressKeysOneFrame(const std::string &keyString) {
    auto keys = ParseKeyString(keyString);
    for (const auto &key : keys) {
        CKKEYBOARD code = GetKeyCode(key);
        if (IsValidKeyCode(code) && code != 0) {
            // Generate press event immediately, release will happen next frame
            m_KeyStates[code].ApplyPressEvent(m_CurrentTick);

            // Schedule release for next frame by setting held duration to 1
            m_HeldKeys[code] = 1;
        }
    }
}

void InputSystem::HoldKeys(const std::string &keyString, int durationTicks) {
    if (durationTicks <= 0) return;

    auto keys = ParseKeyString(keyString);
    for (const auto &key : keys) {
        CKKEYBOARD code = GetKeyCode(key);
        if (IsValidKeyCode(code) && code != 0) {
            // Generate press event now
            m_KeyStates[code].ApplyPressEvent(m_CurrentTick);

            // Schedule release after duration
            m_HeldKeys[code] = durationTicks;
        }
    }
}

void InputSystem::ReleaseKeys(const std::string &keyString) {
    auto keys = ParseKeyString(keyString);
    for (const auto &key : keys) {
        CKKEYBOARD code = GetKeyCode(key);
        if (IsValidKeyCode(code) && code != 0) {
            // Generate release event for this frame
            m_KeyStates[code].ApplyReleaseEvent(m_CurrentTick);

            // Remove from held keys if it was being held
            m_HeldKeys.erase(code);
        }
    }
}

void InputSystem::ReleaseAllKeys() {
    // Generate release events for all currently pressed keys
    for (auto &keyState : m_KeyStates) {
        if (keyState.currentState & KS_PRESSED) {
            keyState.ApplyReleaseEvent(m_CurrentTick);
        }
    }

    m_HeldKeys.clear();
}

// ===================================================================
//  Mouse Control Methods
// ===================================================================

void InputSystem::PressMouseButton(int buttonIndex) {
    if (buttonIndex < 0 || buttonIndex >= 4) return;
    m_MouseState.buttons[buttonIndex].ApplyPressEvent(m_CurrentTick);
}

void InputSystem::PressMouseButtonOneFrame(int buttonIndex) {
    if (buttonIndex < 0 || buttonIndex >= 4) return;
    m_MouseState.buttons[buttonIndex].ApplyPressEvent(m_CurrentTick);
    m_HeldMouseButtons[buttonIndex] = 1;
}

void InputSystem::HoldMouseButton(int buttonIndex, int durationTicks) {
    if (buttonIndex < 0 || buttonIndex >= 4 || durationTicks <= 0) return;
    m_MouseState.buttons[buttonIndex].ApplyPressEvent(m_CurrentTick);
    m_HeldMouseButtons[buttonIndex] = durationTicks;
}

void InputSystem::ReleaseMouseButton(int buttonIndex) {
    if (buttonIndex < 0 || buttonIndex >= 4) return;
    m_MouseState.buttons[buttonIndex].ApplyReleaseEvent(m_CurrentTick);
    m_HeldMouseButtons.erase(buttonIndex);
}

void InputSystem::ReleaseAllMouseButtons() {
    for (auto &btn : m_MouseState.buttons) {
        if (btn.pressed) {
            btn.ApplyReleaseEvent(m_CurrentTick);
        }
    }
    m_HeldMouseButtons.clear();
}

void InputSystem::SetMousePosition(float x, float y) {
    m_MouseState.position.x = x;
    m_MouseState.position.y = y;
}

void InputSystem::MoveMouseRelative(float dx, float dy) {
    m_MouseState.position.x += dx;
    m_MouseState.position.y += dy;
}

void InputSystem::SetMouseWheel(int delta) {
    m_MouseState.wheelDelta = delta;
    m_MouseState.wheelPosition += delta;
}

bool InputSystem::IsMouseButtonDown(int buttonIndex) const {
    if (buttonIndex < 0 || buttonIndex >= 4) return false;
    return m_MouseState.buttons[buttonIndex].pressed;
}

bool InputSystem::IsMouseButtonUp(int buttonIndex) const {
    if (buttonIndex < 0 || buttonIndex >= 4) return false;
    return !m_MouseState.buttons[buttonIndex].pressed;
}

Vx2DVector InputSystem::GetMousePosition() const {
    return m_MouseState.position;
}

int InputSystem::GetMouseWheelDelta() const {
    return m_MouseState.wheelDelta;
}

// ===================================================================
//  Joystick Control Methods
// ===================================================================

void InputSystem::PressJoystickButton(int joystickIndex, int buttonIndex) {
    if (joystickIndex < 0 || buttonIndex < 0) return;

    auto &joyState = m_JoystickStates[joystickIndex];
    if (joyState.buttons.size() <= static_cast<size_t>(buttonIndex)) {
        joyState.buttons.resize(buttonIndex + 1);
    }
    joyState.buttons[buttonIndex].ApplyPressEvent(m_CurrentTick);
}

void InputSystem::PressJoystickButtonOneFrame(int joystickIndex, int buttonIndex) {
    if (joystickIndex < 0 || buttonIndex < 0) return;

    auto &joyState = m_JoystickStates[joystickIndex];
    if (joyState.buttons.size() <= static_cast<size_t>(buttonIndex)) {
        joyState.buttons.resize(buttonIndex + 1);
    }
    joyState.buttons[buttonIndex].ApplyPressEvent(m_CurrentTick);

    int key = (joystickIndex << 16) | buttonIndex;
    m_HeldJoystickButtons[key] = 1;
}

void InputSystem::HoldJoystickButton(int joystickIndex, int buttonIndex, int durationTicks) {
    if (joystickIndex < 0 || buttonIndex < 0 || durationTicks <= 0) return;

    auto &joyState = m_JoystickStates[joystickIndex];
    if (joyState.buttons.size() <= static_cast<size_t>(buttonIndex)) {
        joyState.buttons.resize(buttonIndex + 1);
    }
    joyState.buttons[buttonIndex].ApplyPressEvent(m_CurrentTick);

    int key = (joystickIndex << 16) | buttonIndex;
    m_HeldJoystickButtons[key] = durationTicks;
}

void InputSystem::ReleaseJoystickButton(int joystickIndex, int buttonIndex) {
    if (joystickIndex < 0 || buttonIndex < 0) return;

    auto it = m_JoystickStates.find(joystickIndex);
    if (it == m_JoystickStates.end()) return;

    auto &joyState = it->second;
    if (static_cast<size_t>(buttonIndex) < joyState.buttons.size()) {
        joyState.buttons[buttonIndex].ApplyReleaseEvent(m_CurrentTick);
    }

    int key = (joystickIndex << 16) | buttonIndex;
    m_HeldJoystickButtons.erase(key);
}

void InputSystem::ReleaseAllJoystickButtons(int joystickIndex) {
    if (joystickIndex == -1) {
        // Release all buttons on all joysticks
        for (auto &pair : m_JoystickStates) {
            for (auto &btn : pair.second.buttons) {
                if (btn.pressed) {
                    btn.ApplyReleaseEvent(m_CurrentTick);
                }
            }
        }
        m_HeldJoystickButtons.clear();
    } else {
        // Release all buttons on specific joystick
        auto it = m_JoystickStates.find(joystickIndex);
        if (it != m_JoystickStates.end()) {
            for (size_t i = 0; i < it->second.buttons.size(); ++i) {
                if (it->second.buttons[i].pressed) {
                    it->second.buttons[i].ApplyReleaseEvent(m_CurrentTick);
                }
                int key = (joystickIndex << 16) | static_cast<int>(i);
                m_HeldJoystickButtons.erase(key);
            }
        }
    }
}

void InputSystem::SetJoystickPosition(int joystickIndex, float x, float y, float z) {
    if (joystickIndex < 0) return;
    auto &joyState = m_JoystickStates[joystickIndex];
    joyState.position.x = x;
    joyState.position.y = y;
    joyState.position.z = z;
}

void InputSystem::SetJoystickRotation(int joystickIndex, float rx, float ry, float rz) {
    if (joystickIndex < 0) return;
    auto &joyState = m_JoystickStates[joystickIndex];
    joyState.rotation.x = rx;
    joyState.rotation.y = ry;
    joyState.rotation.z = rz;
}

void InputSystem::SetJoystickSliders(int joystickIndex, float slider0, float slider1) {
    if (joystickIndex < 0) return;
    auto &joyState = m_JoystickStates[joystickIndex];
    joyState.sliders.x = slider0;
    joyState.sliders.y = slider1;
}

void InputSystem::SetJoystickPOV(int joystickIndex, float angle) {
    if (joystickIndex < 0) return;
    auto &joyState = m_JoystickStates[joystickIndex];
    joyState.pov = angle;
}

bool InputSystem::IsJoystickButtonDown(int joystickIndex, int buttonIndex) const {
    if (joystickIndex < 0 || buttonIndex < 0) return false;

    auto it = m_JoystickStates.find(joystickIndex);
    if (it == m_JoystickStates.end()) return false;

    const auto &joyState = it->second;
    if (static_cast<size_t>(buttonIndex) >= joyState.buttons.size()) return false;

    return joyState.buttons[buttonIndex].pressed;
}

bool InputSystem::IsJoystickButtonUp(int joystickIndex, int buttonIndex) const {
    if (joystickIndex < 0 || buttonIndex < 0) return false;

    auto it = m_JoystickStates.find(joystickIndex);
    if (it == m_JoystickStates.end()) return true; // No joystick means button is up

    const auto &joyState = it->second;
    if (static_cast<size_t>(buttonIndex) >= joyState.buttons.size()) return true;

    return !joyState.buttons[buttonIndex].pressed;
}

VxVector InputSystem::GetJoystickPosition(int joystickIndex) const {
    VxVector result = {0, 0, 0};
    if (joystickIndex < 0) return result;

    auto it = m_JoystickStates.find(joystickIndex);
    if (it != m_JoystickStates.end()) {
        result = it->second.position;
    }
    return result;
}

VxVector InputSystem::GetJoystickRotation(int joystickIndex) const {
    VxVector result = {0, 0, 0};
    if (joystickIndex < 0) return result;

    auto it = m_JoystickStates.find(joystickIndex);
    if (it != m_JoystickStates.end()) {
        result = it->second.rotation;
    }
    return result;
}

Vx2DVector InputSystem::GetJoystickSliders(int joystickIndex) const {
    Vx2DVector result = {0, 0};
    if (joystickIndex < 0) return result;

    auto it = m_JoystickStates.find(joystickIndex);
    if (it != m_JoystickStates.end()) {
        result = it->second.sliders;
    }
    return result;
}

float InputSystem::GetJoystickPOV(int joystickIndex) const {
    if (joystickIndex < 0) return -1.0f;

    auto it = m_JoystickStates.find(joystickIndex);
    if (it != m_JoystickStates.end()) {
        return it->second.pov;
    }
    return -1.0f;
}

void InputSystem::Reset() {
    m_CurrentTick = 0;

    for (auto &keyState : m_KeyStates) {
        keyState.Reset();
    }

    m_MouseState.Reset();

    for (auto &pair : m_JoystickStates) {
        pair.second.Reset();
    }

    m_HeldKeys.clear();
    m_HeldMouseButtons.clear();
    m_HeldJoystickButtons.clear();
}

bool InputSystem::AreKeysDown(const std::string &keyString) const {
    auto keys = ParseKeyString(keyString);
    if (keys.empty()) return false;

    for (const auto &key : keys) {
        CKKEYBOARD code = GetKeyCode(key);
        if (!IsValidKeyCode(code) || code == 0) return false;

        if (!(m_KeyStates[code].currentState & KS_PRESSED)) {
            return false;
        }
    }

    return true;
}

bool InputSystem::AreKeysUp(const std::string &keyString) const {
    auto keys = ParseKeyString(keyString);
    if (keys.empty()) return false;

    for (const auto &key : keys) {
        CKKEYBOARD code = GetKeyCode(key);
        if (!IsValidKeyCode(code) || code == 0) return false;

        if (m_KeyStates[code].currentState != KS_IDLE) {
            return false;
        }
    }

    return true;
}

bool InputSystem::AreKeysToggled(const std::string &keyString) const {
    auto keys = ParseKeyString(keyString);
    if (keys.empty()) return false;

    for (const auto &key : keys) {
        CKKEYBOARD code = GetKeyCode(key);
        if (!IsValidKeyCode(code) || code == 0) return false;

        if (!(m_KeyStates[code].currentState & KS_RELEASED)) {
            return false;
        }
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

// ===================================================================
//  Diagnostic Methods
// ===================================================================

std::vector<CKKEYBOARD> InputSystem::GetPressedKeys() const {
    std::vector<CKKEYBOARD> pressedKeys;

    for (int code = 0; code < 256; ++code) {
        if (m_KeyStates[code].currentState & KS_PRESSED) {
            pressedKeys.push_back(static_cast<CKKEYBOARD>(code));
        }
    }

    return pressedKeys;
}

std::vector<int> InputSystem::GetPressedMouseButtons() const {
    std::vector<int> pressedButtons;

    for (size_t i = 0; i < m_MouseState.buttons.size(); ++i) {
        if (m_MouseState.buttons[i].pressed) {
            pressedButtons.push_back(static_cast<int>(i));
        }
    }

    return pressedButtons;
}

std::map<int, std::vector<int>> InputSystem::GetPressedJoystickButtons() const {
    std::map<int, std::vector<int>> pressedButtons;

    for (const auto &pair : m_JoystickStates) {
        int joyIndex = pair.first;
        const JoystickState &joyState = pair.second;

        std::vector<int> buttons;
        for (size_t i = 0; i < joyState.buttons.size(); ++i) {
            if (joyState.buttons[i].pressed) {
                buttons.push_back(static_cast<int>(i));
            }
        }

        if (!buttons.empty()) {
            pressedButtons[joyIndex] = buttons;
        }
    }

    return pressedButtons;
}

bool InputSystem::HasConflicts(const InputSystem &other,
                              std::vector<std::string> *outConflicts) const {
    bool hasConflicts = false;

    // Check keyboard conflicts
    for (int code = 0; code < 256; ++code) {
        bool thisPressed = m_KeyStates[code].currentState & KS_PRESSED;
        bool otherPressed = other.m_KeyStates[code].currentState & KS_PRESSED;

        if (thisPressed && otherPressed) {
            hasConflicts = true;
            if (outConflicts) {
                // Find key name for this code
                std::string keyName;
                for (const auto &pair : m_Keymap) {
                    if (pair.second == code) {
                        keyName = pair.first;
                        break;
                    }
                }
                if (keyName.empty()) {
                    keyName = "key_" + std::to_string(code);
                }
                outConflicts->push_back("Keyboard conflict: " + keyName);
            }
        }
    }

    // Check mouse button conflicts
    for (size_t i = 0; i < m_MouseState.buttons.size(); ++i) {
        if (m_MouseState.buttons[i].pressed && other.m_MouseState.buttons[i].pressed) {
            hasConflicts = true;
            if (outConflicts) {
                std::string buttonName;
                switch (i) {
                    case 0: buttonName = "left"; break;
                    case 1: buttonName = "right"; break;
                    case 2: buttonName = "middle"; break;
                    case 3: buttonName = "X1"; break;
                    default: buttonName = "button_" + std::to_string(i); break;
                }
                outConflicts->push_back("Mouse button conflict: " + buttonName);
            }
        }
    }

    // Check mouse position conflicts (if both set positions)
    if ((m_MouseState.position.x != 0 || m_MouseState.position.y != 0) &&
        (other.m_MouseState.position.x != 0 || other.m_MouseState.position.y != 0)) {
        if (m_MouseState.position.x != other.m_MouseState.position.x ||
            m_MouseState.position.y != other.m_MouseState.position.y) {
            hasConflicts = true;
            if (outConflicts) {
                outConflicts->push_back("Mouse position conflict");
            }
        }
    }

    // Check joystick button conflicts
    for (const auto &pair : m_JoystickStates) {
        int joyIndex = pair.first;
        const JoystickState &thisJoyState = pair.second;

        auto it = other.m_JoystickStates.find(joyIndex);
        if (it != other.m_JoystickStates.end()) {
            const JoystickState &otherJoyState = it->second;

            size_t minSize = (std::min)(thisJoyState.buttons.size(), otherJoyState.buttons.size());
            for (size_t i = 0; i < minSize; ++i) {
                if (thisJoyState.buttons[i].pressed && otherJoyState.buttons[i].pressed) {
                    hasConflicts = true;
                    if (outConflicts) {
                        std::ostringstream oss;
                        oss << "Joystick " << joyIndex << " button " << i << " conflict";
                        outConflicts->push_back(oss.str());
                    }
                }
            }
        }
    }

    return hasConflicts;
}

void InputSystem::Apply(size_t currentTick, DX8InputManager *inputManager) {
    if (!inputManager || !m_Enabled) {
        return;
    }

    m_CurrentTick = currentTick;

    // ===================================================================
    // Step 1: Process held keys timers and generate press/release events
    // ===================================================================

    // Process held keyboard keys
    for (auto it = m_HeldKeys.begin(); it != m_HeldKeys.end();) {
        auto key = it->first;
        int &ticks = it->second;

        if (ticks == 1) {
            // This is the last frame - generate release event
            m_KeyStates[key].ApplyReleaseEvent(currentTick);
            it = m_HeldKeys.erase(it);
        } else {
            --ticks;
            ++it;
        }
    }

    // Process held mouse buttons
    for (auto it = m_HeldMouseButtons.begin(); it != m_HeldMouseButtons.end();) {
        int button = it->first;
        int &ticks = it->second;

        if (ticks == 1) {
            m_MouseState.buttons[button].ApplyReleaseEvent(currentTick);
            it = m_HeldMouseButtons.erase(it);
        } else {
            --ticks;
            ++it;
        }
    }

    // Process held joystick buttons
    for (auto it = m_HeldJoystickButtons.begin(); it != m_HeldJoystickButtons.end();) {
        int key = it->first;
        int &ticks = it->second;

        if (ticks == 1) {
            int joyIndex = (key >> 16);
            int btnIndex = (key & 0xFFFF);
            auto jit = m_JoystickStates.find(joyIndex);
            if (jit != m_JoystickStates.end() && static_cast<size_t>(btnIndex) < jit->second.buttons.size()) {
                jit->second.buttons[btnIndex].ApplyReleaseEvent(currentTick);
            }
            it = m_HeldJoystickButtons.erase(it);
        } else {
            --ticks;
            ++it;
        }
    }

    // ===================================================================
    // Step 2: Apply keyboard state using DX8InputManager
    // ===================================================================

    for (int code = 0; code < 256; ++code) {
        if (m_KeyStates[code].currentState & KS_PRESSED) {
            inputManager->SetKeyDown(static_cast<CKDWORD>(code));
        } else if (m_KeyStates[code].currentState == KS_IDLE) {
            inputManager->SetKeyUp(static_cast<CKDWORD>(code));
        }
    }

    // ===================================================================
    // Step 3: Apply mouse state using DX8InputManager
    // ===================================================================

    // Apply mouse button states
    for (size_t i = 0; i < m_MouseState.buttons.size(); ++i) {
        CK_MOUSEBUTTON button = static_cast<CK_MOUSEBUTTON>(i);
        if (m_MouseState.buttons[i].pressed) {
            inputManager->SetMouseButtonDown(button);
        } else {
            inputManager->SetMouseButtonUp(button);
        }
    }

    // Apply mouse position
    inputManager->SetMousePosition(m_MouseState.position);

    // Apply mouse wheel
    if (m_MouseState.wheelDelta != 0) {
        inputManager->SetMouseWheel(m_MouseState.wheelDelta);
    }

    // ===================================================================
    // Step 4: Apply joystick states using DX8InputManager
    // ===================================================================

    for (const auto &pair : m_JoystickStates) {
        int joyIndex = pair.first;
        const JoystickState &joyState = pair.second;

        // Apply joystick button states
        for (size_t i = 0; i < joyState.buttons.size(); ++i) {
            if (joyState.buttons[i].pressed) {
                inputManager->SetJoystickButtonDown(joyIndex, static_cast<int>(i));
            } else {
                inputManager->SetJoystickButtonUp(joyIndex, static_cast<int>(i));
            }
        }

        // Apply joystick position (X, Y, Z axes)
        inputManager->SetJoystickPosition(joyIndex, joyState.position);

        // Apply joystick rotation (RX, RY, RZ axes)
        inputManager->SetJoystickRotation(joyIndex, joyState.rotation);

        // Apply joystick sliders
        inputManager->SetJoystickSliders(joyIndex, joyState.sliders);

        // Apply joystick POV (point-of-view hat)
        inputManager->SetJoystickPOV(joyIndex, joyState.pov);
    }

    // Clear wheel delta after applying (it's a per-frame delta)
    m_MouseState.wheelDelta = 0;

    PrepareNextFrame();
}

void InputSystem::PrepareNextFrame() {
    for (auto &keyState : m_KeyStates) {
        keyState.PrepareNextFrame();
    }
}

void InputSystem::Reset(unsigned char *keyboardState) {
    if (!keyboardState) return;

    // Reset all keys to idle state
    memset(keyboardState, KS_IDLE, 256);
}
