#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "VxMath.h"

#include "Result.h"

using namespace std;

// Forward declarations
class GameInterface;
class ServiceProvider;

/**
 * @brief Savestate data structure
 *
 * Captures complete game state at a specific moment for later restoration.
 */
struct SavestateData {
    // Metadata
    string name;
    string timestamp;
    string levelName;
    int levelNumber;
    string description;

    // Physics state
    VxVector position;
    VxVector velocity;
    VxVector angularVelocity;
    VxQuaternion rotation;

    // RNG state
    vector<uint32_t> rngState;

    // Game state
    int points;
    int lives;
    int sector;
    float srScore;
    float hsScore;

    // Time state
    size_t tick;

    // Serialization
    string ToJson() const;
    static Result<SavestateData> FromJson(const string &json);
};

/**
 * @brief Manages game state saving and loading
 *
 * Provides savestate functionality for quick retries and experimentation.
 * Savestates are stored as JSON files in the savestates/ directory.
 *
 * Thread Safety: Not thread-safe, must be called from game thread only.
 *
 * Example usage:
 * @code
 * auto manager = provider->Resolve<SavestateManager>();
 *
 * // Save current state
 * auto result = manager->SaveState("checkpoint_1");
 * if (result.is_ok()) {
 *     tas.log("State saved successfully");
 * }
 *
 * // Load state
 * auto load_result = manager->LoadState("checkpoint_1");
 * if (load_result.is_ok()) {
 *     tas.log("State loaded successfully");
 * }
 * @endcode
 */
class SavestateManager {
public:
    /**
     * @brief Constructor
     * @param provider Service provider for dependency resolution
     */
    explicit SavestateManager(ServiceProvider *provider);

    ~SavestateManager() = default;

    // Disable copy
    SavestateManager(const SavestateManager &) = delete;
    SavestateManager &operator=(const SavestateManager &) = delete;

    /**
     * @brief Save current game state
     * @param name Savestate name (used as filename)
     * @return Result<void> Success or error
     *
     * Captures current game state and saves to disk.
     * If a savestate with the same name exists, it will be overwritten.
     *
     * @note This should be called when the game is in a stable state
     *       (e.g., not during a level transition)
     */
    Result<void> SaveState(const string &name);

    /**
     * @brief Save current game state with description
     * @param name Savestate name
     * @param description Human-readable description
     * @return Result<void> Success or error
     */
    Result<void> SaveState(const string &name, const string &description);

    /**
     * @brief Load a saved state
     * @param name Savestate name
     * @return Result<void> Success or error
     *
     * Restores game state from disk.
     *
     * @warning This operation may cause brief physics instability.
     *          Consider waiting a few frames after loading.
     */
    Result<void> LoadState(const string &name);

    /**
     * @brief Delete a savestate
     * @param name Savestate name
     * @return Result<void> Success or error
     */
    Result<void> DeleteState(const string &name);

    /**
     * @brief Check if a savestate exists
     * @param name Savestate name
     * @return bool True if exists
     */
    bool StateExists(const string &name) const;

    /**
     * @brief List all available savestates
     * @return Result<vector<string>> List of savestate names
     */
    Result<vector<string>> ListStates() const;

    /**
     * @brief Get savestate metadata
     * @param name Savestate name
     * @return Result<SavestateData> Savestate data (metadata only, no restoration)
     */
    Result<SavestateData> GetStateInfo(const string &name) const;

    /**
     * @brief Get savestates directory path
     * @return string Path to savestates directory
     */
    string GetSavestatesDirectory() const;

private:
    /**
     * @brief Capture current game state
     * @return Result<SavestateData> Captured state data
     */
    Result<SavestateData> CaptureState() const;

    /**
     * @brief Restore game state from data
     * @param data Savestate data to restore
     * @return Result<void> Success or error
     */
    Result<void> RestoreState(const SavestateData &data);

    /**
     * @brief Get file path for savestate
     * @param name Savestate name
     * @return string Full file path
     */
    string GetSavestatePath(const string &name) const;

    /**
     * @brief Validate savestate name
     * @param name Savestate name
     * @return Result<void> Success or error if name is invalid
     */
    Result<void> ValidateName(const string &name) const;

    /**
     * @brief Capture RNG state
     * @return vector<uint32_t> RNG state
     */
    vector<uint32_t> CaptureRNGState() const;

    /**
     * @brief Restore RNG state
     * @param state RNG state to restore
     * @return Result<void> Success or error
     */
    Result<void> RestoreRNGState(const vector<uint32_t> &state);

private:
    ServiceProvider *m_ServiceProvider;
    shared_ptr<GameInterface> m_GameInterface;

    // Cache for faster lookups
    mutable map<string, SavestateData> m_StateCache;
    mutable bool m_CacheValid;

    // Savestates directory
    string m_SavestatesDir;
};
