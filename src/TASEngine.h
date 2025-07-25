#pragma once

#include <memory>
#include <string>
#include <atomic>

#include <sol/sol.hpp>

#include <BML/ILogger.h>

#define BML_TAS_PATH "..\\ModLoader\\TAS\\"

class TASProject;
// Forward declarations of our subsystems and managers
class BallanceTAS;
class ProjectManager;
class InputSystem;
class GameInterface;
class EventManager;

// Script and record execution subsystems
class ScriptExecutor;
class LuaScheduler;
class RecordPlayer;

// Recording subsystems
class Recorder;
class ScriptGenerator;
struct GenerationOptions;

typedef enum TASState {
    TAS_IDLE              = 0,
    TAS_PLAYING           = 0x1,
    TAS_PLAY_PENDING      = 0x2,
    TAS_RECORDING         = 0x4,
    TAS_RECORD_PENDING    = 0x8,
    TAS_TRANSLATING       = 0x10,
    TAS_TRANSLATE_PENDING = 0x20,
} TASState;

/**
 * @enum PlaybackType
 * @brief The type of playback currently active.
 */
enum class PlaybackType {
    None,   // No playback active
    Script, // Lua script playback via ScriptExecutor
    Record  // Binary record playback via RecordPlayer
};

/**
 * @class TASEngine
 * @brief The central coordinator for the BallanceTAS framework.
 *
 * TASEngine now serves as a coordinator between different execution modes:
 * - Script-based TAS (via ScriptExecutor)
 * - Record-based TAS (via RecordPlayer)
 * - Recording (via Recorder)
 *
 * It provides a unified interface for starting/stopping TAS operations regardless
 * of the underlying implementation, while managing the lifecycle and state of
 * all subsystems.
 */
class TASEngine {
public:
    explicit TASEngine(GameInterface *gameInterface);
    ~TASEngine();

    // TASEngine is not copyable or movable
    TASEngine(const TASEngine &) = delete;
    TASEngine &operator=(const TASEngine &) = delete;

    // --- State Queries ---
    bool IsPlaying() const { return (m_State & TAS_PLAYING) != 0; }
    bool IsPendingPlay() const { return (m_State & TAS_PLAY_PENDING) != 0; }
    bool IsRecording() const { return (m_State & TAS_RECORDING) != 0; }
    bool IsPendingRecord() const { return (m_State & TAS_RECORD_PENDING) != 0; }
    bool IsTranslating() const { return (m_State & TAS_TRANSLATING) != 0; }
    bool IsPendingTranslate() const { return (m_State & TAS_TRANSLATE_PENDING) != 0; }
    bool IsIdle() const { return m_State == TAS_IDLE; }
    bool IsShuttingDown() const { return m_ShuttingDown; }

    /**
     * @brief Gets the current playback type.
     * @return The type of playback currently active.
     */
    PlaybackType GetPlaybackType() const { return m_PlaybackType; }

    /**
     * @brief Checks if script playback is active.
     * @return True if playing a script-based TAS.
     */
    bool IsPlayingScript() const { return GetPlaybackType() == PlaybackType::Script; }

    /**
     * @brief Checks if record playback is active.
     * @return True if playing a record-based TAS.
     */
    bool IsPlayingRecord() const { return GetPlaybackType() == PlaybackType::Record; }

    /**
     * @brief Initializes all subsystems.
     * @return True on success, false on failure.
     */
    bool Initialize();

    /**
     * @brief Shuts down all subsystems and releases resources.
     */
    void Shutdown();

    /**
     * @brief Starts the TASEngine, enabling hooks and preparing for execution.
     * This is called when the mod is enabled or when the game starts.
     */
    void Start();

    /**
     * @brief Stops the TASEngine, disabling hooks and cleaning up.
     * This is called when the mod is disabled or when the game ends.
     */
    void Stop();

    /**
     * @brief Handles game events forwarded from BallanceTASMod.
     * @param eventName The name of the event (e.g., "level_start").
     * @param args Optional arguments for the event.
     */
    template <typename... Args>
    void OnGameEvent(const std::string &eventName, Args... args);

    // === Recording Control ===

    /**
     * @brief Sets up recording to start when next level loads.
     * @return True if recording mode was set successfully.
     */
    bool StartRecording();

    /**
     * @brief Stops recording (will auto-generate script if configured).
     */
    void StopRecording();

    /**
     * @brief Gets the current recording frame count.
     * @return Number of frames recorded, or 0 if not recording.
     */
    size_t GetRecordingFrameCount() const;

    // === Unified Replay Control ===

    /**
     * @brief Sets up replay to start when next level loads.
     * Automatically chooses ScriptExecutor or RecordPlayer based on project type.
     * @return True if replay mode was set successfully.
     */
    bool StartReplay();

    /**
     * @brief Stops replay (works for both script and record playback).
     */
    void StopReplay(bool clearProject = false);

    // === Translation Control (Record to Script Conversion) ===

    /**
     * @brief Sets up translation to start when next level loads.
     * Translation simultaneously plays a record and records it to generate a script.
     * @return True if translation mode was set successfully.
     */
    bool StartTranslation();

    /**
     * @brief Stops translation and generates the script.
     */
    void StopTranslation(bool clearProject = false);

    // === Validation Recording Control ===

    /**
     * @brief Starts validation recording during script playback.
     * @param outputPath Base path for validation dumps (without extension).
     * @return True if validation recording was enabled successfully.
     */
    bool StartValidationRecording(const std::string &outputPath);

    /**
     * @brief Stop validation recording and generates validation dumps.
     * @return True if validation dumps were generated successfully.
     */
    bool StopValidationRecording();

    /**
     * @brief Checks if validation recording is currently enabled.
     * @return True if validation recording is active.
     */
    bool IsValidationEnabled() const { return m_ValidationEnabled; }

    /**
     * @brief Sets whether validation recording is enabled.
     * This controls if validation dumps are generated during script playback.
     * @param enabled True to enable validation recording, false to disable.
     */
    void SetValidationEnabled(bool enabled) { m_ValidationEnabled = enabled; }

    /**
     * @brief Gets the current validation output path.
     * @return The validation output path, or empty string if not set.
     */
    const std::string &GetValidationOutputPath() const { return m_ValidationOutputPath; }

    // === Auto-Restart Control ===

    /**
     * @brief Checks if auto-restart is enabled.
     * Auto-restart will automatically restart the current project when it finishes.
     * @return True if auto-restart is enabled, false otherwise.
     */
    bool IsAutoRestartEnabled() const { return m_AutoRestart; }

    /**
     * @brief Sets whether auto-restart is enabled.
     * If enabled, the current project will automatically restart when it finishes.
     * @param enabled True to enable auto-restart, false to disable.
     */
    void SetAutoRestartEnabled(bool enabled) { m_AutoRestart = enabled; }

    /**
     * @brief Restarts the current project if auto-restart is enabled.
     * This will reload the project and start it again.
     * @return True if the project was restarted, false if auto-restart is disabled.
     */
    bool RestartCurrentProject();

    // --- Subsystem Accessors ---
    // These are used by other parts of the framework (e.g., LuaApi) to get handles
    // to the necessary systems.

    GameInterface *GetGameInterface() const { return m_GameInterface; }

    ILogger *GetLogger() const;
    void AddTimer(size_t tick, const std::function<void()> &callback);

    // For Lua API compatibility, delegate to ScriptExecutor
    sol::state &GetLuaState();
    sol::state &GetLuaState() const;
    LuaScheduler *GetScheduler() const;

    ProjectManager *GetProjectManager() const { return m_ProjectManager.get(); }
    InputSystem *GetInputSystem() const { return m_InputSystem.get(); }
    EventManager *GetEventManager() const { return m_EventManager.get(); }

    // Script execution subsystem
    ScriptExecutor *GetScriptExecutor() const { return m_ScriptExecutor.get(); }

    // Record playback subsystem
    RecordPlayer *GetRecordPlayer() const { return m_RecordPlayer.get(); }

    // Recording subsystem accessors
    Recorder *GetRecorder() const { return m_Recorder.get(); }
    ScriptGenerator *GetScriptGenerator() const { return m_ScriptGenerator.get(); }

    // --- Tick Management ---
    size_t GetCurrentTick() const;
    void SetCurrentTick(size_t tick);
    void IncrementCurrentTick() { ++m_CurrentTick; }

    // --- Path Management ---
    const std::string &GetPath() const { return m_Path; }
    void SetPath(const std::string &path) { m_Path = path; }

private:
    /**
     * @brief Internal method to start recording when level loads.
     */
    void StartRecordingInternal();

    /**
     * @brief Internal method to start replay when level loads.
     * Chooses appropriate executor based on project type.
     */
    void StartReplayInternal();

    /**
     * @brief Internal method to start translation when level loads.
     * Sets up both record playback and recording simultaneously.
     */
    void StartTranslationInternal();

    /**
     * @brief Immediately stops recording without timers (for shutdown).
     */
    void StopRecordingImmediate();

    /**
     * @brief Immediately stops replay without timers (for shutdown).
     */
    void StopReplayImmediate();

    /**
     * @brief Immediately stops translation without timers (for shutdown).
     */
    void StopTranslationImmediate();

    /**
     * @brief Clears all registered callbacks to prevent duplicates.
     * Called before registering new callbacks in Start().
     */
    void ClearCallbacks();

    /**
     * @brief Sets up callbacks for recording mode.
     */
    void SetupRecordingCallbacks();

    /**
     * @brief Sets up callbacks for script playback mode.
     */
    void SetupScriptPlaybackCallbacks();

    /**
     * @brief Sets up callbacks for record playback mode.
     */
    void SetupRecordPlaybackCallbacks();

    /**
     * @brief Sets up callbacks for translation mode (combined recording + playback).
     */
    void SetupTranslationCallbacks();

    /**
     * @brief Handles automatic completion when record playback finishes during translation.
     */
    void OnTranslationPlaybackComplete();

    /**
     * @brief Determines the appropriate playback type for a project.
     * @param project The project to analyze.
     * @return The playback type to use.
     */
    PlaybackType DeterminePlaybackType(const TASProject *project) const;

    // State setters (used internally and by callbacks)
    void SetPlayPending(bool pending) {
        if (pending) {
            m_State |= TAS_PLAY_PENDING;
            m_State &= ~TAS_RECORD_PENDING; // Can't play and record simultaneously
        } else {
            m_State &= ~TAS_PLAY_PENDING;
        }
    }

    void SetPlaying(bool playing) {
        if (playing) {
            m_State |= TAS_PLAYING;
            m_State &= ~(TAS_RECORDING | TAS_RECORD_PENDING); // Can't play and record simultaneously
        } else {
            m_State &= ~TAS_PLAYING;
            m_PlaybackType = PlaybackType::None;
        }
    }

    void SetRecordPending(bool pending) {
        if (pending) {
            m_State |= TAS_RECORD_PENDING;
            m_State &= ~TAS_PLAY_PENDING; // Can't record and play simultaneously
        } else {
            m_State &= ~TAS_RECORD_PENDING;
        }
    }

    void SetRecording(bool recording) {
        if (recording) {
            m_State |= TAS_RECORDING;
            m_State &= ~(TAS_PLAYING | TAS_PLAY_PENDING); // Can't record and play simultaneously
        } else {
            m_State &= ~TAS_RECORDING;
        }
    }

    void SetTranslatePending(bool pending) {
        if (pending) {
            m_State |= TAS_TRANSLATE_PENDING;
            m_State &= ~(TAS_PLAY_PENDING | TAS_RECORD_PENDING); // Clear other pending states
        } else {
            m_State &= ~TAS_TRANSLATE_PENDING;
        }
    }

    void SetTranslating(bool translating) {
        if (translating) {
            m_State |= TAS_TRANSLATING;
            m_State &= ~(TAS_PLAYING | TAS_RECORDING); // Clear individual states
        } else {
            m_State &= ~TAS_TRANSLATING;
        }
    }

    GameInterface *m_GameInterface;

    // --- Managers ---
    std::unique_ptr<ProjectManager> m_ProjectManager;
    std::unique_ptr<EventManager> m_EventManager;

    // --- Core Logic Modules ---
    std::unique_ptr<InputSystem> m_InputSystem;

    // --- Execution Modules ---
    std::unique_ptr<ScriptExecutor> m_ScriptExecutor;
    std::unique_ptr<RecordPlayer> m_RecordPlayer;

    // --- Recording Modules ---
    std::unique_ptr<Recorder> m_Recorder;
    std::unique_ptr<ScriptGenerator> m_ScriptGenerator;

    // --- State ---
    uint32_t m_State = TAS_IDLE;
    PlaybackType m_PlaybackType = PlaybackType::None;
    std::atomic<bool> m_ShuttingDown;
    size_t m_CurrentTick = 0;
    std::string m_Path = BML_TAS_PATH;

    bool m_AutoRestart = false; // Automatically restart current project when enter the same level again
    bool m_ValidationEnabled = false;
    bool m_ValidationRecording = false;
    std::string m_ValidationOutputPath;
};
