#pragma once

#include <sol/sol.hpp>
#include <memory>
#include <string>

class TASProject;
// Forward declarations of our subsystems and managers
class BallanceTAS;
class ProjectManager;
class LuaScheduler;
class InputSystem;
class GameInterface;
class DevTools;
class EventManager;

// Recording subsystems
class Recorder;
class ScriptGenerator;
struct GenerationOptions;

typedef enum TASState {
    TAS_IDLE           = 0,
    TAS_PLAYING        = 0x1,
    TAS_PLAY_PENDING   = 0x2,
    TAS_RECORDING      = 0x4, // New recording state
    TAS_RECORD_PENDING = 0x8, // Recording will start when level loads
} TASState;

/**
 * @class TASEngine
 * @brief The central coordinator and execution engine of the BallanceTAS framework.
 *
 * TASEngine owns and manages the lifecycle of all core TAS subsystems. It holds the main
 * Lua VM instance and orchestrates the flow of data and control between the game,
 * the hooks, and the Lua scripts. It now also handles recording and script generation.
 */
class TASEngine {
public:
    explicit TASEngine(BallanceTAS *mod);
    ~TASEngine();

    // TASEngine is not copyable or movable
    TASEngine(const TASEngine &) = delete;
    TASEngine &operator=(const TASEngine &) = delete;

    // --- State Queries ---
    bool IsPlaying() const { return (m_State & TAS_PLAYING) != 0; }
    bool IsPendingPlay() const { return (m_State & TAS_PLAY_PENDING) != 0; }
    bool IsRecording() const { return (m_State & TAS_RECORDING) != 0; }
    bool IsPendingRecord() const { return (m_State & TAS_RECORD_PENDING) != 0; }
    bool IsIdle() const { return m_State == TAS_IDLE; }

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

    // --- High-Level Control ---

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

    /**
     * @brief Loads a TAS project and prepares it for execution.
     * @param project The TAS project to load, containing metadata and scripts.
     * @return True if the project was loaded successfully, false otherwise.
     */
    bool LoadTAS(const TASProject *project);

    /**
     * @brief Unloads the currently active TAS, stopping execution and cleaning up.
     */
    void UnloadTAS();

    // --- Recording Control ---

    /**
     * @brief Sets up recording to start when next level loads.
     * @return True if recording mode was set successfully.
     */
    bool SetupRecording();

    /**
     * @brief Starts actual recording (called internally when level loads).
     * @return True if recording started successfully.
     */
    bool StartRecording();

    /**
     * @brief Stops recording and generates a TAS script.
     * @param projectName Name for the generated project.
     * @param options Optional generation options.
     * @return True if script was generated successfully.
     */
    bool StopRecordingAndGenerate(const std::string &projectName, const GenerationOptions *options = nullptr);

    /**
     * @brief Stops recording without generating a script.
     */
    void StopRecording();

    /**
     * @brief Gets the current recording frame count.
     * @return Number of frames recorded, or 0 if not recording.
     */
    size_t GetRecordingFrameCount() const;

    // --- State & Configuration Setters ---

    void SetDeveloperMode(bool enabled);

    // --- Subsystem Accessors ---
    // These are used by other parts of the framework (e.g., LuaApi) to get handles
    // to the necessary systems.

    BallanceTAS *GetMod() const { return m_Mod; }
    sol::state &GetLuaState() { return m_LuaState; }
    const sol::state &GetLuaState() const { return m_LuaState; }

    ProjectManager *GetProjectManager() const { return m_ProjectManager.get(); }
    LuaScheduler *GetScheduler() const { return m_Scheduler.get(); }
    InputSystem *GetInputSystem() const { return m_InputSystem.get(); }
    GameInterface *GetGameInterface() const { return m_GameInterface.get(); }
    DevTools *GetDevTools() const { return m_DevTools.get(); }
    EventManager *GetEventManager() const { return m_EventManager.get(); }

    // Recording subsystem accessors
    Recorder *GetRecorder() const { return m_Recorder.get(); }
    ScriptGenerator *GetScriptGenerator() const { return m_ScriptGenerator.get(); }

private:
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
     * @brief Sets up callbacks for playback mode.
     */
    void SetupPlaybackCallbacks();

    /**
     * @brief Internal method to handle recording tick.
     */
    void TickRecording();

    BallanceTAS *m_Mod;

    // The single, master Lua virtual machine for all TAS scripts.
    sol::state m_LuaState;

    // --- Managers ---
    std::unique_ptr<ProjectManager> m_ProjectManager;
    std::unique_ptr<EventManager> m_EventManager;

    // --- Core Logic Modules ---
    std::unique_ptr<LuaScheduler> m_Scheduler;
    std::unique_ptr<InputSystem> m_InputSystem;
    std::unique_ptr<GameInterface> m_GameInterface;
    std::unique_ptr<DevTools> m_DevTools;

    // --- Recording Modules ---
    std::unique_ptr<Recorder> m_Recorder;
    std::unique_ptr<ScriptGenerator> m_ScriptGenerator;

    // --- State ---
    uint32_t m_State = TAS_IDLE;
};
