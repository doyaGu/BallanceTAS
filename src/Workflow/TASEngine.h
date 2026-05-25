#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "EventBus.h"
#include "Runtime/OperationRequestStore.h"
#include "PlaybackTypes.h"
#include "Runtime/RuntimeSession.h"
#include "ServiceContainer.h"
#include "TASConstants.h"

extern "C" { struct lua_State; }

class TASProject;
class BallanceTAS;
class ProjectManager;
class InputSystem;
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
class SavestateManager;
class RuntimeEventRouter;
class RuntimeSession;
class ContextLifecycleCoordinator;
class LuaTypedEventBridge;

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

    // State queries
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

    // Facade methods (high-level operations)
    bool StartRecording();
    void StopRecording();
    size_t GetRecordingFrameCount() const;
    bool StartReplay();
    void StopReplay(bool clearProject = false);
    bool StartTranslation();
    void StopTranslation(bool clearProject = false);
    bool StartValidationRecording(const std::string &outputPath);
    bool StopValidationRecording();
    bool RestartCurrentProject();

    // Settings
    bool IsValidationEnabled() const;
    void SetValidationEnabled(bool enabled);
    const std::string &GetValidationOutputPath() const;
    bool IsAutoRestartEnabled() const { return m_AutoRestart; }
    void SetAutoRestartEnabled(bool enabled) { m_AutoRestart = enabled; }
    const std::string &GetLastCompletedPlaybackProjectName() const { return m_LastCompletedPlaybackProjectName; }
    std::string GetLastTranslationResultMessage() const;

    // DI access point
    ServiceProvider &GetServiceProvider() { return m_ServiceProvider; }

    // Tick management
    size_t GetCurrentTick() const;
    void SetCurrentTick(size_t tick);
    void IncrementCurrentTick() { ++m_CurrentTick; }

    const std::string &GetPath() const { return m_Path; }
    void SetPath(const std::string &path) { m_Path = path; }

    // Convenience accessors — delegate to ServiceProvider
    GameInterface *GetGameInterface() const { return m_GameInterface; }
    EventBus *GetEventBus() const { return m_EventBus; }
    HookManager *GetHookManager() const { return m_HookManager; }

    void AddTimer(size_t tick, const std::function<void()> &callback);
    lua_State *GetLuaState() const;
    LuaScheduler *GetScheduler() const;

    // Request store queries (used by state handlers)
    TASProject *GetRequestedProject() const { return m_Requests.requestedProject; }
    PlaybackType GetRequestedPlaybackType() const { return m_Requests.requestedPlaybackType; }
    bool ShouldUseValidationForRecording() const { return m_Requests.requestedValidationRecording; }
    bool ShouldClearProjectOnStop() const { return m_Requests.clearProjectOnStop; }
    void ClearControlRequests();

private:
    RuntimeSession *Session() const;
    bool StopSession(RuntimeSession::StopOptions options, const char *reason);
    void HandlePlaybackCompleted();
    float ResolveLevelLoadPhysicsDeltaTime() const;
    std::string BuildValidationOutputPath(TASProject *project) const;

    // Externally-owned (BallanceTAS lifetime)
    GameInterface *m_GameInterface = nullptr;
    EventBus *m_EventBus = nullptr;
    HookManager *m_HookManager = nullptr;

    // IoC container — owns all subsystems
    ServiceContainer m_ServiceContainer;
    ServiceProvider m_ServiceProvider{m_ServiceContainer};

    // Engine's own state
    OperationRequestStore m_Requests;
    std::atomic<bool> m_ShuttingDown = false;
    size_t m_CurrentTick = 0;
    std::string m_Path = TASConstants::DefaultBasePath;
    std::string m_LastCompletedPlaybackProjectName;
    bool m_AutoRestart = false;
    bool m_ValidationEnabled = false;
};
