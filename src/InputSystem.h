#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>
#include <map>

#include <CKInputManager.h>

// Exact state representation
struct KeyState {
    uint8_t currentState = KS_IDLE; // Current accumulated state
    size_t timestamp = 0;           // Event timestamp

    // Apply events
    void ApplyPressEvent(size_t ts) {
        currentState |= KS_PRESSED;
        timestamp = ts;
    }

    void ApplyReleaseEvent(size_t ts) {
        currentState = (currentState & KS_PRESSED)
            ? static_cast<uint8_t>(KS_PRESSED | KS_RELEASED)
            : static_cast<uint8_t>(KS_RELEASED);
        timestamp = ts;
    }

    void Reset() {
        currentState = KS_IDLE;
        timestamp = 0;
    }

    // Reset for new frame (mimics PostProcess cleanup)
    void PrepareNextFrame() {
        if (currentState & KS_RELEASED) {
            currentState = KS_IDLE;
        }
    }

};

// Joystick button state tracking
struct JoystickButtonState {
    bool pressed = false;
    size_t timestamp = 0;

    void ApplyPressEvent(size_t ts) {
        pressed = true;
        timestamp = ts;
    }

    void ApplyReleaseEvent(size_t ts) {
        pressed = false;
        timestamp = ts;
    }

    void Reset() {
        pressed = false;
        timestamp = 0;
    }
};

// Joystick state tracking
struct JoystickState {
    std::vector<JoystickButtonState> buttons; // Dynamic based on capabilities
    VxVector position;
    VxVector rotation;
    Vx2DVector sliders;
    float pov = -1.0f; // -1 means centered

    void Reset() {
        for (auto &btn : buttons) {
            btn.Reset();
        }
        position.x = position.y = position.z = 0.0f;
        rotation.x = rotation.y = rotation.z = 0.0f;
        sliders.x = sliders.y = 0.0f;
        pov = -1.0f;
    }
};

/**
 * @class InputSystem
 * @brief A minimal-state, preemptive input control system for TAS replay.
 *
 * When enabled, this system takes complete control of keyboard input,
 * overriding ALL physical keyboard input to ensure deterministic TAS playback.
 * When disabled, the system does not interfere with input at all.
 *
 * The system maintains minimal state for timed key holds but has no complex
 * frame-to-frame dependencies. All timing logic is self-contained within
 * the Apply() method.
 *
 * Key behaviors:
 * - ENABLED: Completely overrides ALL input during TAS replay
 * - DISABLED: Does not touch keyboard state at all
 * - MINIMAL STATE: Only tracks current key states and simple timers
 */
class InputSystem {
public:
    struct ExplicitKeyboardState {
        std::array<bool, 256> isSet{};
        std::array<unsigned char, 256> values{};

        bool Has(size_t keyCode) const { return keyCode < isSet.size() && isSet[keyCode]; }
    };

    InputSystem();
    ~InputSystem() = default;

    // InputSystem is not copyable or movable
    InputSystem(const InputSystem &) = delete;
    InputSystem &operator=(const InputSystem &) = delete;

    // --- API for Lua Bindings ---

    /**
     * @brief Immediately presses the specified key(s).
     * @param keyString The case-insensitive key name(s). Can be single key ("up")
     *                  or combination ("up right", "up,right", "up;right").
     */
    void PressKeys(const std::string &keyString);

    /**
     * @brief Presses key(s) for exactly one frame, then automatically releases them.
     * @param keyString The key name(s) to press for one frame.
     */
    void PressKeysOneFrame(const std::string &keyString);

    /**
     * @brief Holds key(s) for a specified number of frames, then automatically releases them.
     * @param keyString The key name(s) to hold.
     * @param durationTicks The number of frames to hold the keys.
     */
    void HoldKeys(const std::string &keyString, int durationTicks);

    /**
     * @brief Immediately releases the specified key(s).
     * @param keyString The key name(s). Supports combinations.
     */
    void ReleaseKeys(const std::string &keyString);

    /**
     * @brief Immediately releases all keys currently pressed by the TAS system.
     */
    void ReleaseAllKeys();

    // --- Joystick Control API ---

    /**
     * @brief Immediately presses the specified joystick button.
     * @param joystickIndex The joystick index (0-based).
     * @param buttonIndex The button index.
     */
    void PressJoystickButton(int joystickIndex, int buttonIndex);

    /**
     * @brief Presses joystick button for exactly one frame, then automatically releases it.
     * @param joystickIndex The joystick index (0-based).
     * @param buttonIndex The button index.
     */
    void PressJoystickButtonOneFrame(int joystickIndex, int buttonIndex);

    /**
     * @brief Holds joystick button for a specified number of frames, then automatically releases it.
     * @param joystickIndex The joystick index (0-based).
     * @param buttonIndex The button index.
     * @param durationTicks The number of frames to hold the button.
     */
    void HoldJoystickButton(int joystickIndex, int buttonIndex, int durationTicks);

    /**
     * @brief Immediately releases the specified joystick button.
     * @param joystickIndex The joystick index (0-based).
     * @param buttonIndex The button index.
     */
    void ReleaseJoystickButton(int joystickIndex, int buttonIndex);

    /**
     * @brief Releases all joystick buttons on a specific joystick.
     * @param joystickIndex The joystick index (0-based). If -1, releases all buttons on all joysticks.
     */
    void ReleaseAllJoystickButtons(int joystickIndex = -1);

    /**
     * @brief Sets the joystick axis position.
     * @param joystickIndex The joystick index (0-based).
     * @param x The X axis position.
     * @param y The Y axis position.
     * @param z The Z axis position.
     */
    void SetJoystickPosition(int joystickIndex, float x, float y, float z);

    /**
     * @brief Sets the joystick rotation axes.
     * @param joystickIndex The joystick index (0-based).
     * @param rx The X rotation.
     * @param ry The Y rotation.
     * @param rz The Z rotation.
     */
    void SetJoystickRotation(int joystickIndex, float rx, float ry, float rz);

    /**
     * @brief Sets the joystick slider positions.
     * @param joystickIndex The joystick index (0-based).
     * @param slider0 The first slider position.
     * @param slider1 The second slider position.
     */
    void SetJoystickSliders(int joystickIndex, float slider0, float slider1);

    /**
     * @brief Sets the joystick POV (point-of-view) angle.
     * @param joystickIndex The joystick index (0-based).
     * @param angle The POV angle in degrees, or -1 for centered.
     */
    void SetJoystickPOV(int joystickIndex, float angle);

    /**
     * @brief Checks if a joystick button is currently pressed by the TAS system.
     * @param joystickIndex The joystick index (0-based).
     * @param buttonIndex The button index.
     * @return True if the button is currently pressed.
     */
    bool IsJoystickButtonDown(int joystickIndex, int buttonIndex) const;

    /**
     * @brief Checks if a joystick button is currently released by the TAS system.
     * @param joystickIndex The joystick index (0-based).
     * @param buttonIndex The button index.
     * @return True if the button is currently released.
     */
    bool IsJoystickButtonUp(int joystickIndex, int buttonIndex) const;

    /**
     * @brief Gets the current joystick position.
     * @param joystickIndex The joystick index (0-based).
     * @return The joystick position as VxVector.
     */
    VxVector GetJoystickPosition(int joystickIndex) const;

    /**
     * @brief Gets the current joystick rotation.
     * @param joystickIndex The joystick index (0-based).
     * @return The joystick rotation as VxVector.
     */
    VxVector GetJoystickRotation(int joystickIndex) const;

    /**
     * @brief Gets the current joystick slider positions.
     * @param joystickIndex The joystick index (0-based).
     * @return The slider positions as Vx2DVector.
     */
    Vx2DVector GetJoystickSliders(int joystickIndex) const;

    /**
     * @brief Gets the current joystick POV angle.
     * @param joystickIndex The joystick index (0-based).
     * @return The POV angle, or -1 if centered.
     */
    float GetJoystickPOV(int joystickIndex) const;

    /**
     * @brief Resets the InputSystem state, clearing all key states.
     * This is called at the start of each frame to prepare for new input.
     */
    void Reset();

    /**
     * @brief Sets whether the InputSystem should take complete control of input.
     * When enabled, the system completely overrides ALL keyboard input.
     * @param enabled True to enable preemptive control (TAS replay mode).
     */
    void SetEnabled(bool enabled) { m_Enabled = enabled; }

    /**
     * @brief Checks if preemptive control is currently enabled.
     * @return True if InputSystem is overriding all input.
     */
    bool IsEnabled() const { return m_Enabled; }

    // --- Query Methods ---

    /**
     * @brief Checks if a key name is valid and supported.
     * @param key The single key name to check.
     * @return True if the key is valid.
     */
    bool IsValidKey(const std::string &key) const;

    /**
     * @brief Checks if key(s) are currently pressed by the TAS system.
     * @param keyString The key name(s) to check. For combinations, returns true if ALL keys are pressed.
     * @return True if all specified keys are currently pressed by TAS.
     */
    bool AreKeysDown(const std::string &keyString) const;

    /**
     * @brief Checks if key(s) are currently released by the TAS system.
     * @param keyString The key name(s) to check. For combinations, returns true if ALL keys are released.
     * @return True if all specified keys are currently released by TAS.
     */
    bool AreKeysUp(const std::string &keyString) const;

    /**
     * @brief Checks if key(s) are currently toggled (pressed and released).
     * @param keyString The key name(s) to check. For combinations, returns true if ALL keys are toggled.
     * @return True if all specified keys are toggled (pressed and released).
     */
    bool AreKeysToggled(const std::string &keyString) const;

    /**
     * @brief Gets a list of all available key names.
     * @return A sorted vector of all supported key names.
     */
    std::vector<std::string> GetAvailableKeys() const;

    // --- Diagnostic Methods (for multi-context merging and conflict detection) ---

    /**
     * @brief Gets the complete keyboard state for diagnostics/merging.
     * @return Reference to the internal keyboard state array.
     */
    const std::array<KeyState, 256> &GetAllKeyStates() const { return m_KeyStates; }

    /**
     * @brief Gets all joystick states for diagnostics/merging.
     * @return Reference to the internal joystick states map.
     */
    const std::map<int, JoystickState> &GetAllJoystickStates() const { return m_JoystickStates; }

    /**
     * @brief Gets a list of all currently pressed keys.
     * @return Vector of CKKEYBOARD codes for all pressed keys.
     */
    std::vector<CKKEYBOARD> GetPressedKeys() const;

    /**
     * @brief Gets a list of all currently pressed joystick buttons.
     * @return Map of joystick index -> vector of pressed button indices.
     */
    std::map<int, std::vector<int>> GetPressedJoystickButtons() const;

    /**
     * @brief Checks for input conflicts with another InputSystem (for diagnostics).
     * @param other The other InputSystem to compare against.
     * @param outConflicts Optional output parameter to receive conflict details.
     * @return True if there are any conflicting inputs.
     */
    bool HasConflicts(const InputSystem &other,
                     std::vector<std::string> *outConflicts = nullptr) const;

    // --- Core Method for Hooking ---

    /**
     * @brief Applies TAS keyboard input through CKInputManager's keyboard state buffer.
     * @param currentTick The current game tick
     * @param inputManager Pointer to the CKInputManager instance
     */
    void Apply(size_t currentTick, CKInputManager *inputManager);

    /**
     * @brief Applies TAS keyboard input to a raw 256-byte Virtools keyboard state buffer.
     * @param currentTick The current game tick
     * @param keyboardState Pointer to the keyboard state buffer returned by CKInputManager.
     */
    void ApplyToKeyboardState(size_t currentTick, unsigned char *keyboardState);

    /**
     * @brief Captures only the keys explicitly controlled by this context for the current tick.
     */
    ExplicitKeyboardState CaptureExplicitKeyboardState(size_t currentTick);

    /**
     * @brief Applies a merged high-to-low priority list of context inputs once to a keyboard buffer.
     */
    static void ApplyMergedToKeyboardState(size_t currentTick,
                                           const std::vector<InputSystem *> &inputsHighToLow,
                                           unsigned char *keyboardState);

    /**
     * @brief Prepares for next frame
     */
    void PrepareNextFrame();

    /**
     * @brief Resets the game's keyboard state buffer.
     * @param keyboardState The game's keyboard state buffer to reset.
     */
    static void Reset(unsigned char *keyboardState);

private:
    /**
     * @brief Parses a key string into individual key names.
     * Supports space, comma, and semicolon separators.
     * @param keyString The input string like "up right" or "up,right"
     * @return Vector of individual key names
     */
    std::vector<std::string> ParseKeyString(const std::string &keyString) const;

    /**
     * @brief Converts a string key name to a CKKEYBOARD code.
     * @param key The string representation of the key.
     * @return The CKKEYBOARD enum value, or 0 if not found.
     */
    CKKEYBOARD GetKeyCode(const std::string &key) const;

    /**
     * @brief Initializes the key mapping table.
     */
    void InitializeKeyMap();

    /**
     * @brief Checks if a CKKEYBOARD code is valid for array indexing.
     * @param keyCode The key code to validate.
     * @return True if the key code is within valid range.
     */
    static bool IsValidKeyCode(CKKEYBOARD keyCode);

    void ProcessHeldInputs(size_t currentTick);

    // A map from string key name to its corresponding BML key code.
    std::unordered_map<std::string, CKKEYBOARD> m_Keymap;

    // Keyboard state tracking
    std::array<KeyState, 256> m_KeyStates;

    // Joystick state tracking (indexed by joystick ID)
    std::map<int, JoystickState> m_JoystickStates;

    // Current frame tracking
    size_t m_CurrentTick = 0;

    // Keys being held for a specific duration (key -> remaining ticks)
    std::unordered_map<CKKEYBOARD, int> m_HeldKeys;

    // Joystick buttons being held for a specific duration ((joystick_id << 16 | button) -> remaining ticks)
    std::unordered_map<int, int> m_HeldJoystickButtons;

    // Whether the system should override ALL input
    bool m_Enabled = false;
};
