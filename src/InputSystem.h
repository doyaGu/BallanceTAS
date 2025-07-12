#pragma once

#include <string>
#include <set>
#include <unordered_map>
#include <vector>

#include <CKInputManager.h>

// Exact state representation
struct KeyState {
    uint8_t currentState = KS_IDLE; // Current accumulated state
    bool hadPressEvent = false;     // KS_PRESSED was added this frame
    bool hadReleaseEvent = false;   // KS_RELEASED was added this frame
    size_t timestamp = 0;           // Event timestamp

    // Reset for new frame (mimics PostProcess cleanup)
    void PrepareNextFrame() {
        if (currentState & KS_RELEASED) {
            currentState = KS_IDLE;
        }
        hadPressEvent = false;
        hadReleaseEvent = false;
    }

    // Apply events (mimics PreProcess accumulation)
    void ApplyPressEvent(size_t ts) {
        currentState |= KS_PRESSED;
        hadPressEvent = true;
        timestamp = ts;
    }

    void ApplyReleaseEvent(size_t ts) {
        currentState |= KS_RELEASED;
        hadReleaseEvent = true;
        timestamp = ts;
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
     * @brief Checks if key(s) are currently being pressed by the TAS system.
     * @param keyString The key name(s) to check. For combinations, returns true if ALL keys are pressed.
     * @return True if all specified keys are currently pressed by TAS.
     */
    bool AreKeysPressed(const std::string &keyString) const;

    /**
     * @brief Gets a list of all available key names.
     * @return A sorted vector of all supported key names.
     */
    std::vector<std::string> GetAvailableKeys() const;

    // --- Core Method for Hooking ---

    /**
     * @brief Applies TAS input by replicating state transitions
     * This now properly handles state accumulation and frame lifecycle
     */
    void Apply(size_t currentTick, unsigned char *keyboardState);

    /**
     * @brief Prepares for next frame (mimics PostProcess cleanup)
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

    // A map from string key name to its corresponding BML key code.
    std::unordered_map<std::string, CKKEYBOARD> m_Keymap;

    // State tracking
    std::unordered_map<CKKEYBOARD, KeyState> m_KeyStates;

    // Current frame tracking
    size_t m_CurrentTick = 0;

    // Keys being held for a specific duration (key -> remaining ticks)
    std::unordered_map<CKKEYBOARD, int> m_HeldKeys;

    // Whether the system should override ALL input
    bool m_Enabled = false;
};
