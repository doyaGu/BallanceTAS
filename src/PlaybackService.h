#pragma once

#include "Result.h"
#include "HookManager.h"
#include "EventBus.h"
#include "PlaybackTypes.h"

// Forward declarations
class ServiceProvider;
class TASProject;
class ScriptContextManager;
class RecordPlayer;
class Recorder;
class InputSystem;
class GameInterface;
class ProjectManager;
class DX8InputManager;
class CKInputManager;
struct StartLevelEvent;
#ifdef ENABLE_REPL
class LuaREPLServer;
#endif

/**
 * @class PlaybackService
 * @brief Owns the full playback lifecycle for both script and record playback.
 *
 * Replaces: PlaybackController + ScriptPlaybackStrategy + RecordPlaybackStrategy
 *           + IPlaybackStrategy interface.
 *
 * Internally delegates to ScriptContextManager (script) or RecordPlayer (record)
 * depending on PlaybackType. Uses HookManager for per-frame callbacks with
 * RAII-guarded ScopedCallbacks. The state machine owns pending/active truth and
 * drives prepare/activate/stop.
 */
class PlaybackService {
public:
    explicit PlaybackService(ServiceProvider *provider);
    ~PlaybackService();

    PlaybackService(const PlaybackService &) = delete;
    PlaybackService &operator=(const PlaybackService &) = delete;

    void SetEventBus(EventBus *bus);
    void SetHookManager(HookManager *hookMgr);

    /**
     * @brief Begin a playback session (deferred until level loads).
     * @param project  The TAS project to play.
     * @param type     Script or Record playback.
     */
    Result<void> PreparePlayback(TASProject *project, PlaybackType type);

    /**
     * @brief Activate deferred playback once the level is loaded.
     */
    Result<void> ActivatePlayback();

    /**
     * @brief Stop the current playback session.
     * @param clearProject  If true, the project reference is cleared.
     */
    Result<void> StopPlaybackGraceful(bool clearProject = true);

    /** Immediately stop without cleanup callbacks (for shutdown paths). */
    void StopPlaybackImmediate();

    void Pause();
    void Resume();

    // --- Queries ---
    bool IsPrepared() const { return m_IsPrepared; }
    bool IsPlaying() const { return m_IsPlaying; }
    bool IsPaused() const { return m_IsPaused; }
    PlaybackType GetPlaybackType() const { return m_Type; }
    float GetProgress() const;
    size_t GetCurrentTick() const { return m_CurrentTick; }
    TASProject *GetCurrentProject() const { return m_CurrentProject; }

    /**
     * @brief Callback type invoked when playback finishes naturally.
     */
    using CompletionCallback = std::function<void()>;
    void SetCompletionCallback(CompletionCallback cb) { m_CompletionCallback = std::move(cb); }

private:
    // Activation helpers (called from ActivatePlayback)
    Result<void> ActivateScriptPlayback();
    Result<void> ActivateRecordPlayback();

    // Hook installation
    void InstallScriptCallbacks();
    void InstallRecordCallbacks();
    void RemoveHookCallbacks();

    // Input merging (script playback — multi-context priority system)
    void ApplyMergedContextInputs(DX8InputManager *inputManager);

    // Input system management
    void SetupInputSystem();
    void CleanupInputSystem();

    ServiceProvider *m_ServiceProvider;
    EventBus *m_EventBus = nullptr;
    HookManager *m_HookManager = nullptr;

    // Cached subsystems
    ScriptContextManager *m_ScriptManager = nullptr;
    RecordPlayer *m_RecordPlayer = nullptr;
    Recorder *m_Recorder = nullptr;
    InputSystem *m_InputSystem = nullptr;
    GameInterface *m_GameInterface = nullptr;

    // State
    TASProject *m_CurrentProject = nullptr;
    PlaybackType m_Type = PlaybackType::None;
    bool m_IsPrepared = false;
    bool m_IsPlaying = false;
    bool m_IsPaused = false;
    bool m_CompletionSignaled = false;
    size_t m_CurrentTick = 0;

    // RAII hook guards
    ScopedCallback m_PostTickGuard;
    ScopedCallback m_PostInputGuard;

    CompletionCallback m_CompletionCallback;
};
