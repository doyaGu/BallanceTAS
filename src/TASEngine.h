#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "EventBus.h"
#include "PlaybackTypes.h"
#include "TASStateMachine.h"

extern "C" { struct lua_State; }

class ServiceContainer;
class ServiceProvider;

#define BML_TAS_PATH "..\\ModLoader\\TAS\\"

class TASProject;
class BallanceTAS;
class ProjectManager;
class InputSystem;
class DX8InputManager;
class GameInterface;
class EventManager;
class ScriptContextManager;
class ScriptContext;
class LuaScheduler;
class RecordPlayer;
class StartupProjectManager;
class LuaREPLServer;
class Recorder;
class ScriptGenerator;
class RecordingService;
class PlaybackService;
class TranslationService;
class ValidationService;
class EventBus;
class HookManager;

struct StartLevelEvent;
struct PlaybackCompletedEvent;
struct TranslationCompletedEvent;

class TASEngine {
    friend class EngineBootstrap;
public:
    explicit TASEngine(GameInterface *gameInterface, EventBus *eventBus, HookManager *hookManager);
    ~TASEngine();

    TASEngine(const TASEngine &) = delete;
    TASEngine &operator=(const TASEngine &) = delete;

    bool Initialize();
    void Shutdown();
    void Start();
    void Stop();

    bool IsPlaying() const;
    bool IsRecording() const;
    bool IsTranslating() const;
    bool IsIdle() const;
    bool IsPaused() const;
    bool IsPlayingScript() const;
    bool IsPlayingRecord() const;
    bool IsShuttingDown() const { return m_ShuttingDown; }

    PlaybackType GetPlaybackType() const;

    bool IsPendingPlay() const;
    bool IsPendingRecord() const;
    bool IsPendingTranslate() const;

    bool StartRecording();
    void StopRecording();
    size_t GetRecordingFrameCount() const;

    bool StartReplay();
    void StopReplay(bool clearProject = false);

    bool StartTranslation();
    void StopTranslation(bool clearProject = false);

    bool StartValidationRecording(const std::string &outputPath);
    bool StopValidationRecording();
    bool IsValidationEnabled() const;
    void SetValidationEnabled(bool enabled);
    const std::string &GetValidationOutputPath() const;

    bool IsAutoRestartEnabled() const { return m_AutoRestart; }
    void SetAutoRestartEnabled(bool enabled) { m_AutoRestart = enabled; }
    bool RestartCurrentProject();

    GameInterface *GetGameInterface() const { return m_GameInterface; }
    void AddTimer(size_t tick, const std::function<void()> &callback);

    lua_State *GetLuaState() const;
    LuaScheduler *GetScheduler() const;

    ProjectManager *GetProjectManager() const;
    InputSystem *GetInputSystem() const;
    EventManager *GetEventManager() const;
    ScriptContextManager *GetScriptContextManager() const;
#ifdef ENABLE_REPL
    LuaREPLServer *GetREPLServer() const;
#endif
    RecordPlayer *GetRecordPlayer() const;
    Recorder *GetRecorder() const;
    ScriptGenerator *GetScriptGenerator() const;
    StartupProjectManager *GetStartupProjectManager() const;
    ServiceProvider *GetServiceProvider() const;
    TASStateMachine *GetStateMachine() const;

    RecordingService *GetRecordingService() const { return m_RecordingService; }
    PlaybackService *GetPlaybackService() const { return m_PlaybackService; }
    TranslationService *GetTranslationService() const { return m_TranslationService; }
    ValidationService *GetValidationService() const { return m_ValidationService; }
    EventBus *GetEventBus() const { return m_EventBus; }
    HookManager *GetHookManager() const { return m_HookManager; }

    size_t GetCurrentTick() const;
    void SetCurrentTick(size_t tick);
    void IncrementCurrentTick() { ++m_CurrentTick; }
    const std::string &GetPath() const { return m_Path; }
    void SetPath(const std::string &path) { m_Path = path; }

    TASProject *GetRequestedProject() const { return m_RequestedProject; }
    PlaybackType GetRequestedPlaybackType() const { return m_RequestedPlaybackType; }
    bool ShouldUseValidationForRecording() const { return m_RequestedValidationRecording; }
    bool ShouldClearProjectOnStop() const { return m_ClearProjectOnStop; }
    void ClearControlRequests();

private:
    void RegisterEventSubscriptions();
    void EnsureGlobalContext();
    void EnsureLevelContext();
    void DestroyLevelContexts();
    void BridgeLuaEvent(const std::string &eventName, std::optional<int> eventData = std::nullopt);
    void HandleStartLevelEvent(const StartLevelEvent &event);
    void HandlePlaybackCompletedEvent(const PlaybackCompletedEvent &event);
    void HandleTranslationCompletedEvent(const TranslationCompletedEvent &event);
    bool TransitionState(TASStateMachine::Event event, const char *reason);
    std::string GetCurrentLevelName() const;
    std::string BuildValidationOutputPath(TASProject *project) const;

    GameInterface *m_GameInterface = nullptr;

    std::unique_ptr<ServiceContainer> m_ServiceContainer;
    mutable std::unique_ptr<ServiceProvider> m_ServiceProvider;

    InputSystem *m_InputSystem = nullptr;
    EventManager *m_EventManager = nullptr;
    Recorder *m_Recorder = nullptr;
    ScriptGenerator *m_ScriptGenerator = nullptr;
    ScriptContextManager *m_ScriptContextManager = nullptr;
    RecordPlayer *m_RecordPlayer = nullptr;
    StartupProjectManager *m_StartupProjectManager = nullptr;
    ProjectManager *m_ProjectManager = nullptr;
    TASStateMachine *m_StateMachine = nullptr;
#ifdef ENABLE_REPL
    LuaREPLServer *m_REPLServer = nullptr;
#endif

    RecordingService *m_RecordingService = nullptr;
    PlaybackService *m_PlaybackService = nullptr;
    TranslationService *m_TranslationService = nullptr;
    ValidationService *m_ValidationService = nullptr;
    EventBus *m_EventBus = nullptr;
    HookManager *m_HookManager = nullptr;

    std::vector<ScopedSubscription> m_EventSubscriptions;

    TASProject *m_RequestedProject = nullptr;
    PlaybackType m_RequestedPlaybackType = PlaybackType::None;
    bool m_RequestedValidationRecording = false;
    bool m_ClearProjectOnStop = false;

    std::atomic<bool> m_ShuttingDown = false;
    size_t m_CurrentTick = 0;
    std::string m_Path = BML_TAS_PATH;
    bool m_AutoRestart = false;
    bool m_ValidationEnabled = false;
};
