#pragma once

#include <BML/BMLAll.h>

#include <string>
#include <set>
#include <map>
#include <unordered_map>
#include <vector>

/**
 * @class InputSystem
 * @brief Manages a virtual keyboard state controlled by TAS scripts.
 *
 * This class receives high-level input commands from Lua (e.g., press, hold)
 * and maintains an internal representation of the desired keyboard state for each
 * upcoming frame. On each game tick, its Apply() method is called by a hook,
 * where it overwrites the game's native keyboard state buffer with its own,
 * thus achieving frame-perfect input injection.
 *
 * Supports key combinations like "up right", "up,right", "up;right" for all APIs.
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
     * @brief Simulates a key press for a single frame.
     * @param keyString The case-insensitive key name(s). Can be single key ("up")
     *                  or combination ("up right", "up,right", "up;right").
     */
    void Press(const std::string &keyString);

    /**
     * @brief Simulates holding key(s) down for a specified number of frames.
     * The keys are automatically released on the (duration_ticks + 1)-th frame.
     * @param keyString The key name(s). Supports combinations.
     * @param durationTicks The number of frames to hold the key(s).
     */
    void Hold(const std::string &keyString, int durationTicks);

    /**
     * @brief Simulates pressing key(s) down indefinitely.
     * The keys will remain pressed until explicitly released with KeyUp.
     * @param keyString The key name(s). Supports combinations.
     */
    void KeyDown(const std::string &keyString);

    /**
     * @brief Releases key(s) that were previously pressed with KeyDown.
     * @param keyString The key name(s). Supports combinations.
     */
    void KeyUp(const std::string &keyString);

    /**
     * @brief Immediately releases all keys currently held by the script.
     * This is a crucial cleanup tool.
     */
    void ReleaseAllKeys();

    /**
     * @brief Resets the entire input system state to default.
     * Called when a TAS is unloaded.
     */
    void Reset();

    // --- Query Methods ---

    /**
     * @brief Checks if a key name is valid and supported.
     * @param key The single key name to check.
     * @return True if the key is valid.
     */
    bool IsValidKey(const std::string &key) const;

    /**
     * @brief Checks if key(s) are currently being pressed by the system.
     * @param keyString The key name(s) to check. For combinations, returns true if ALL keys are pressed.
     * @return True if all specified keys are currently pressed.
     */
    bool IsKeyPressed(const std::string &keyString) const;

    /**
     * @brief Gets a list of all available key names.
     * @return A sorted vector of all supported key names.
     */
    std::vector<std::string> GetAvailableKeys() const;

    // --- Core Method for Hooking ---

    /**
     * @brief Applies the virtual keyboard state to the game's actual keyboard buffer.
     * This is the "synthesis" step, called by HookManager every frame.
     * @param keyboardState A pointer to the game's keyboard state buffer.
     */
    void Apply(unsigned char *keyboardState);

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
    static bool IsValidKeyCode(CKKEYBOARD keyCode) ;

    // Represents a request to hold a key for a certain duration.
    struct HoldRequest {
        int remainingTicks;
    };

    // A map from string key name to its corresponding BML key code.
    std::unordered_map<std::string, CKKEYBOARD> m_Keymap;

    // --- Virtual Keyboard State ---

    // Keys requested via tas.press(), active for only one 'Apply' call.
    std::set<CKKEYBOARD> m_PressedKeys;

    // Keys requested via tas.hold(), managed with a tick countdown.
    std::map<CKKEYBOARD, HoldRequest> m_HeldKeys;

    // Keys requested via tas.key_down(), managed manually by the script.
    std::set<CKKEYBOARD> m_DownKeys;

    // Track keys we controlled in the previous frame for clean releases
    std::set<CKKEYBOARD> m_PreviouslyControlledKeys;
};