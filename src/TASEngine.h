#pragma once

#include <memory>
#include <string>
#include <atomic>

#include <sol/sol.hpp>

#include <BML/ILogger.h>

// Forward declare TASStateMachine to avoid circular dependency
class TASStateMachine;

// Forward declare Controllers (Phase 2.3)
class RecordingController;
class PlaybackController;
class TranslationController;

#define BML_TAS_PATH "..\\ModLoader\\TAS\\"

class TASProject;
// Forward declarations of our subsystems and managers
class BallanceTAS;
class ProjectManager;
class InputSystem;
class DX8InputManager;
class GameInterface;
class EventManager;

// Script and record execution subsystems
class ScriptContextManager;  // Multi-context script system
class ScriptContext;
class LuaScheduler;
class RecordPlayer;

// Startup script management
class StartupProjectManager;

// Recording subsystems
class Recorder;
class ScriptGenerator;
struct GenerationOptions;

/**
 * @enum PlaybackType
 * @brief The type of playback currently active.
 */
enum class PlaybackType {
    None,   // No playback active
    Script, // Lua script playback via ScriptContextManager
    Record  // Binary record playback via RecordPlayer
};

/**
 * @enum PendingOperation
 * @brief Tracks operations waiting for level load to complete.
 *
 * Used to defer TAS operations until the game reaches a stable state
 * (e.g., waiting for level_start event before starting recording/playback).
 */
enum class PendingOperation {
    None,            // No pending operation
    StartRecording,  // Waiting to start recording
    StartPlaying,    // Waiting to start playback (Script or Record)
    StartTranslation // Waiting to start translation (Record to Script)
};

/**
 * @class TASEngine
 * @brief The central coordinator for the BallanceTAS framework.
 *
 * TASEngine serves as a coordinator between different execution modes:
 * - Script-based TAS (via ScriptContextManager)
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
    // Active state queries (using StateMachine - defined in .cpp to avoid forward declaration issues)
    bool IsPlaying() const;
    bool IsRecording() const;
    bool IsTranslating() const;
    bool IsIdle() const;
    bool IsPaused() const;

    // Pending state queries (using PendingOperation enum)
    bool IsPendingPlay() const { return m_PendingOperation == PendingOperation::StartPlaying; }
    bool IsPendingRecord() const { return m_PendingOperation == PendingOperation::StartRecording; }
    bool IsPendingTranslate() const { return m_PendingOperation == PendingOperation::StartTranslation; }

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
    bool IsPlayingScript() const;

    /**
     * @brief Checks if record playback is active.
     * @return True if playing a record-based TAS.
     */
    bool IsPlayingRecord() const;

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
     * Automatically chooses ScriptContextManager or RecordPlayer based on project type.
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
    void AddTimer(size_t tick, const std::function<void()> &callback);

    // For Lua API compatibility, delegate to current context
    sol::state &GetLuaState();
    sol::state &GetLuaState() const;
    LuaScheduler *GetScheduler() const;

    ProjectManager *GetProjectManager() const { return m_ProjectManager.get(); }
    InputSystem *GetInputSystem() const { return m_InputSystem.get(); }
    EventManager *GetEventManager() const { return m_EventManager.get(); }

    // Script execution subsystem
    ScriptContextManager *GetScriptContextManager() const { return m_ScriptContextManager.get(); }
#ifdef ENABLE_REPL
    LuaREPLServer *GetREPLServer() const { return m_REPLServer.get(); }
#endif

    // Record playback subsystem
    RecordPlayer *GetRecordPlayer() const { return m_RecordPlayer.get(); }

    // Recording subsystem accessors
    Recorder *GetRecorder() const { return m_Recorder.get(); }
    ScriptGenerator *GetScriptGenerator() const { return m_ScriptGenerator.get(); }

    // Startup script management accessors
    StartupProjectManager *GetStartupProjectManager() const { return m_StartupProjectManager.get(); }

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
     * @brief Handles context lifecycle events (creating/destroying contexts).
     * @param eventName The name of the game event.
     */
    void HandleContextLifecycleEvent(const std::string &eventName);

    /**
     * @brief Gets the current level name from GameInterface.
     * @return The current level name, or empty string if not available.
     */
    std::string GetCurrentLevelName() const;

    /**
     * @brief Merges and applies inputs from all active contexts by priority.
     * Higher priority contexts override lower priority contexts.
     * @param inputManager The input manager to apply merged inputs to.
     */
    void ApplyMergedContextInputs(DX8InputManager *inputManager);

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

    // State setters (used internally and by callbacks - defined in .cpp to avoid forward declaration issues)
    void SetPlayPending(bool pending);
    void SetPlaying(bool playing);
    void SetRecordPending(bool pending);
    void SetRecording(bool recording);
    void SetTranslatePending(bool pending);
    void SetTranslating(bool translating);

    GameInterface *m_GameInterface;

    // --- Managers ---
    std::unique_ptr<ProjectManager> m_ProjectManager;
    std::unique_ptr<EventManager> m_EventManager;

    // --- Core Logic Modules ---
    std::unique_ptr<InputSystem> m_InputSystem;

    // --- Execution Modules ---
    std::unique_ptr<ScriptContextManager> m_ScriptContextManager;  // Multi-context script system
#ifdef ENABLE_REPL
    std::unique_ptr<LuaREPLServer> m_REPLServer;                   // Remote Lua REPL for debugging
#endif
    std::unique_ptr<RecordPlayer> m_RecordPlayer;

    // --- Recording Modules ---
    std::unique_ptr<Recorder> m_Recorder;
    std::unique_ptr<ScriptGenerator> m_ScriptGenerator;

    // --- Startup Script Module ---
    std::unique_ptr<StartupProjectManager> m_StartupProjectManager;

    // --- State ---
    PlaybackType m_PlaybackType = PlaybackType::None;
    PendingOperation m_PendingOperation = PendingOperation::None;  // Operation waiting for level load
    std::atomic<bool> m_ShuttingDown;
    size_t m_CurrentTick = 0;
    std::string m_Path = BML_TAS_PATH;

    // State Machine (Phase 2 refactoring)
    std::unique_ptr<TASStateMachine> m_StateMachine;

    // Controllers (Phase 2.3 refactoring)
    std::unique_ptr<RecordingController> m_RecordingController;
    std::unique_ptr<PlaybackController> m_PlaybackController;
    std::unique_ptr<TranslationController> m_TranslationController;

    bool m_AutoRestart = false; // Automatically restart current project when enter the same level again
    bool m_ValidationEnabled = false;
    bool m_ValidationRecording = false;
    std::string m_ValidationOutputPath;
};
