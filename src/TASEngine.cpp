#include "TASEngine.h"

#include <filesystem>
#include <sol/sol.hpp>

#include "EngineBootstrap.h"
#include "EventManager.h"
#include "GameEvents.h"
#include "GameInterface.h"
#include "HookManager.h"
#include "InputSystem.h"
#include "Logger.h"
#include "PlaybackService.h"
#include "ProjectManager.h"
#include "RecordPlayer.h"
#include "Recorder.h"
#include "RecordingService.h"
#include "ScriptContext.h"
#include "ScriptContextManager.h"
#include "ScriptGenerator.h"
#include "ServiceContainer.h"
#include "StartupProjectManager.h"
#include "TASProject.h"
#include "TASStateMachine.h"
#include "TranslationService.h"
#include "ValidationService.h"

#ifdef ENABLE_REPL
#include "LuaREPLServer.h"
#endif

namespace fs = std::filesystem;

TASEngine::TASEngine(GameInterface *gameInterface, EventBus *eventBus, HookManager *hookManager)
    : m_GameInterface(gameInterface),
      m_EventBus(eventBus),
      m_HookManager(hookManager) {
    if (!m_GameInterface || !m_EventBus || !m_HookManager) {
        throw std::runtime_error("TASEngine requires valid game and runtime infrastructure.");
    }
}

TASEngine::~TASEngine() {
    if (!m_ShuttingDown) {
        Shutdown();
    }
}

bool TASEngine::Initialize() {
    if (m_ShuttingDown) {
        Log::Error("Cannot initialize TASEngine during shutdown.");
        return false;
    }

    if (!EngineBootstrap::InitializeCoreSubsystems(*this)) {
        return false;
    }

    m_InputSystem = m_ServiceContainer->Resolve<InputSystem>();
    m_EventManager = m_ServiceContainer->Resolve<EventManager>();
    m_Recorder = m_ServiceContainer->Resolve<Recorder>();
    m_ScriptGenerator = m_ServiceContainer->Resolve<ScriptGenerator>();
    m_ScriptContextManager = m_ServiceContainer->Resolve<ScriptContextManager>();
    m_RecordPlayer = m_ServiceContainer->Resolve<RecordPlayer>();
    m_StartupProjectManager = m_ServiceContainer->Resolve<StartupProjectManager>();
    m_StateMachine = m_ServiceContainer->Resolve<TASStateMachine>();

    m_RecordingService = m_ServiceContainer->Resolve<RecordingService>();
    m_PlaybackService = m_ServiceContainer->Resolve<PlaybackService>();
    m_TranslationService = m_ServiceContainer->Resolve<TranslationService>();
    m_ValidationService = m_ServiceContainer->Resolve<ValidationService>();
    if (auto *eventBus = m_ServiceContainer->Resolve<EventBus>()) {
        m_EventBus = eventBus;
    }
    if (auto *hookManager = m_ServiceContainer->Resolve<HookManager>()) {
        m_HookManager = hookManager;
    }

    if (!EngineBootstrap::InitializeHighLevelSubsystems(*this)) {
        return false;
    }

    m_ProjectManager = m_ServiceContainer->Resolve<ProjectManager>();
#ifdef ENABLE_REPL
    m_REPLServer = m_ServiceContainer->Resolve<LuaREPLServer>();
#endif

    RegisterEventSubscriptions();

    Log::Info("TASEngine initialization complete.");
    return true;
}

void TASEngine::Shutdown() {
    const bool wasShuttingDown = m_ShuttingDown.exchange(true);
    if (wasShuttingDown) {
        return;
    }

    bool handledByStateMachine = false;
    if (m_StateMachine && !m_StateMachine->IsShuttingDown()) {
        auto result = m_StateMachine->Transition(TASStateMachine::Event::Shutdown);
        if (!result.IsOk()) {
            Log::Error("Failed to transition to shutdown state: %s",
                       result.GetError().message.c_str());
        } else {
            handledByStateMachine = true;
        }
    }

    if (!handledByStateMachine) {
        if (m_ValidationService && m_ValidationService->IsActive()) {
            m_ValidationService->StopImmediate();
        }
        if (m_PlaybackService) {
            m_PlaybackService->StopPlaybackImmediate();
        }
        if (m_RecordingService) {
            m_RecordingService->StopRecordingImmediate();
        }
        if (m_TranslationService) {
            m_TranslationService->StopTranslationImmediate();
        }
    }

    m_EventSubscriptions.clear();

#ifdef ENABLE_REPL
    if (m_REPLServer) {
        m_REPLServer->Shutdown();
    }
#endif

    if (m_ScriptContextManager) {
        m_ScriptContextManager->Shutdown();
    }

    m_ServiceProvider.reset();
    if (m_ServiceContainer) {
        m_ServiceContainer->Clear();
    }

    m_InputSystem = nullptr;
    m_EventManager = nullptr;
    m_Recorder = nullptr;
    m_ScriptGenerator = nullptr;
    m_ScriptContextManager = nullptr;
    m_RecordPlayer = nullptr;
    m_StartupProjectManager = nullptr;
    m_ProjectManager = nullptr;
    m_StateMachine = nullptr;
    m_RecordingService = nullptr;
    m_PlaybackService = nullptr;
    m_TranslationService = nullptr;
    m_ValidationService = nullptr;
#ifdef ENABLE_REPL
    m_REPLServer = nullptr;
#endif

    m_GameInterface = nullptr;
    Log::Info("TASEngine shutdown complete.");
}

void TASEngine::Start() {
    if (m_ShuttingDown || !m_GameInterface) {
        return;
    }

    AddTimer(1ul, [this]() {
        if (m_GameInterface) {
            m_GameInterface->ResetPhysicsTime();
        }
    });
}

void TASEngine::Stop() {
    if (m_ShuttingDown || !m_StateMachine || m_StateMachine->IsIdle() || m_StateMachine->IsShuttingDown()) {
        return;
    }

    bool shouldKeepGlobalScript = false;
    if (IsPlayingScript() && m_PlaybackService) {
        auto *project = m_PlaybackService->GetCurrentProject();
        shouldKeepGlobalScript = project && project->IsGlobalProject();
    }

    if (shouldKeepGlobalScript) {
        if (m_ValidationService && m_ValidationService->IsActive()) {
            StopValidationRecording();
        }
        Log::Info("Keeping global script playback active during level transition.");
        return;
    }

    m_ClearProjectOnStop = true;
    const bool transitioned = TransitionState(TASStateMachine::Event::LevelEnd, "level end");
    if (transitioned && IsAutoRestartEnabled()) {
        RestartCurrentProject();
    }
}

bool TASEngine::StartRecording() {
    if (m_ShuttingDown || !m_StateMachine || !m_StateMachine->IsIdle()) {
        Log::Warn("Cannot start recording: TAS is not idle.");
        return false;
    }

    ClearControlRequests();
    m_RequestedValidationRecording = false;
    return TransitionState(TASStateMachine::Event::StartRecording, "start recording");
}

void TASEngine::StopRecording() {
    if (m_ShuttingDown) {
        if (m_RecordingService) {
            m_RecordingService->StopRecordingImmediate();
        }
        return;
    }

    if (!m_StateMachine || (!m_StateMachine->IsRecording() && !m_StateMachine->IsPendingRecord())) {
        return;
    }

    m_ClearProjectOnStop = false;
    TransitionState(TASStateMachine::Event::Stop, "stop recording");
}

size_t TASEngine::GetRecordingFrameCount() const {
    return m_RecordingService ? m_RecordingService->GetFrameCount() : 0;
}

bool TASEngine::StartReplay() {
    if (m_ShuttingDown || !m_StateMachine || !m_StateMachine->IsIdle()) {
        Log::Warn("Cannot start replay: TAS is not idle.");
        return false;
    }
    if (!m_ProjectManager) {
        Log::Error("ProjectManager not available.");
        return false;
    }

    TASProject *project = m_ProjectManager->GetCurrentProject();
    if (!project || !project->IsValid()) {
        Log::Error("No valid TAS project selected.");
        return false;
    }

    PlaybackType type = PlaybackType::None;
    if (project->IsScriptProject()) {
        type = PlaybackType::Script;
    } else if (project->IsRecordProject()) {
        type = PlaybackType::Record;
    }

    if (type == PlaybackType::None) {
        Log::Error("Unable to determine playback type for project: %s", project->GetName().c_str());
        return false;
    }

    ClearControlRequests();
    m_RequestedProject = project;
    m_RequestedPlaybackType = type;

    return TransitionState(
        type == PlaybackType::Script
            ? TASStateMachine::Event::StartScriptPlayback
            : TASStateMachine::Event::StartRecordPlayback,
        "start replay"
    );
}

void TASEngine::StopReplay(bool clearProject) {
    if (m_ShuttingDown) {
        if (m_ValidationService && m_ValidationService->IsActive()) {
            m_ValidationService->StopImmediate();
        }
        if (m_PlaybackService) {
            m_PlaybackService->StopPlaybackImmediate();
        }
        return;
    }

    if (!m_StateMachine ||
        (!m_StateMachine->IsPlaying() && !m_StateMachine->IsPaused() && !m_StateMachine->IsPendingPlay())) {
        return;
    }

    m_ClearProjectOnStop = clearProject;
    TransitionState(TASStateMachine::Event::Stop, "stop replay");
}

bool TASEngine::StartTranslation() {
    if (m_ShuttingDown || !m_StateMachine || !m_StateMachine->IsIdle()) {
        Log::Warn("Cannot start translation: TAS is not idle.");
        return false;
    }
    if (!m_ProjectManager) {
        Log::Error("ProjectManager not available.");
        return false;
    }

    TASProject *project = m_ProjectManager->GetCurrentProject();
    if (!project) {
        Log::Error("No project selected for translation.");
        return false;
    }

    ClearControlRequests();
    m_RequestedProject = project;

    return TransitionState(TASStateMachine::Event::StartTranslation, "start translation");
}

void TASEngine::StopTranslation(bool clearProject) {
    if (m_ShuttingDown) {
        if (m_TranslationService) {
            m_TranslationService->StopTranslationImmediate();
        }
        return;
    }

    if (!m_StateMachine ||
        (!m_StateMachine->IsTranslating() && !m_StateMachine->IsPendingTranslate())) {
        return;
    }

    m_ClearProjectOnStop = clearProject;
    TransitionState(TASStateMachine::Event::Stop, "stop translation");
}

bool TASEngine::StartValidationRecording(const std::string &outputPath) {
    if (!m_ValidationService || !m_PlaybackService) {
        return false;
    }

    auto result = m_ValidationService->Start(outputPath, *m_PlaybackService);
    if (!result.IsOk()) {
        Log::Error("Validation recording: %s", result.GetError().message.c_str());
        return false;
    }
    return true;
}

bool TASEngine::StopValidationRecording() {
    if (!m_ValidationService || !m_ValidationService->IsActive()) {
        return false;
    }

    auto result = m_ValidationService->Stop();
    if (!result.IsOk()) {
        Log::Error("Validation recording stop: %s", result.GetError().message.c_str());
        return false;
    }
    return true;
}

bool TASEngine::IsValidationEnabled() const {
    return m_ValidationEnabled;
}

void TASEngine::SetValidationEnabled(bool enabled) {
    m_ValidationEnabled = enabled;
}

const std::string &TASEngine::GetValidationOutputPath() const {
    static const std::string empty;
    return m_ValidationService ? m_ValidationService->GetOutputPath() : empty;
}

bool TASEngine::RestartCurrentProject() {
    if (m_ShuttingDown || !m_ProjectManager) {
        Log::Warn("Cannot restart during shutdown or without ProjectManager.");
        return false;
    }

    TASProject *project = m_ProjectManager->GetCurrentProject();
    if (!project || !project->IsValid()) {
        Log::Error("No valid project to restart.");
        return false;
    }

    Log::Info("Restarting TAS project: %s", project->GetName().c_str());
    return StartReplay();
}

bool TASEngine::IsPlaying() const {
    return m_StateMachine && m_StateMachine->IsPlaying();
}

bool TASEngine::IsRecording() const {
    return m_StateMachine && m_StateMachine->IsRecording();
}

bool TASEngine::IsTranslating() const {
    return m_StateMachine && m_StateMachine->IsTranslating();
}

bool TASEngine::IsIdle() const {
    return m_StateMachine && m_StateMachine->IsIdle();
}

bool TASEngine::IsPaused() const {
    return m_StateMachine && m_StateMachine->IsPaused();
}

bool TASEngine::IsPlayingScript() const {
    return (IsPlaying() || IsPaused()) && GetPlaybackType() == PlaybackType::Script;
}

bool TASEngine::IsPlayingRecord() const {
    return (IsPlaying() || IsPaused()) && GetPlaybackType() == PlaybackType::Record;
}

PlaybackType TASEngine::GetPlaybackType() const {
    if (!m_StateMachine) {
        return PlaybackType::None;
    }

    switch (m_StateMachine->GetCurrentState()) {
    case TASStateMachine::State::PendingScriptPlayback:
    case TASStateMachine::State::PlayingScript:
        return PlaybackType::Script;
    case TASStateMachine::State::PendingRecordPlayback:
    case TASStateMachine::State::PlayingRecord:
        return PlaybackType::Record;
    case TASStateMachine::State::Paused:
        switch (m_StateMachine->GetPreviousState()) {
        case TASStateMachine::State::PlayingScript:
            return PlaybackType::Script;
        case TASStateMachine::State::PlayingRecord:
            return PlaybackType::Record;
        default:
            break;
        }
        break;
    default:
        break;
    }

    return m_PlaybackService ? m_PlaybackService->GetPlaybackType() : PlaybackType::None;
}

bool TASEngine::IsPendingPlay() const {
    return m_StateMachine && m_StateMachine->IsPendingPlay();
}

bool TASEngine::IsPendingRecord() const {
    return m_StateMachine && m_StateMachine->IsPendingRecord();
}

bool TASEngine::IsPendingTranslate() const {
    return m_StateMachine && m_StateMachine->IsPendingTranslate();
}

void TASEngine::RegisterEventSubscriptions() {
    if (!m_EventBus) {
        return;
    }

    m_EventSubscriptions.clear();

    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PreStartMenuEvent>(
        [this](const PreStartMenuEvent &) { BridgeLuaEvent(PreStartMenuEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PostStartMenuEvent>(
        [this](const PostStartMenuEvent &) {
            EnsureGlobalContext();
            BridgeLuaEvent(PostStartMenuEvent::name);
        }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PreLoadLevelEvent>(
        [this](const PreLoadLevelEvent &) { BridgeLuaEvent(PreLoadLevelEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PostLoadLevelEvent>(
        [this](const PostLoadLevelEvent &) { BridgeLuaEvent(PostLoadLevelEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<StartLevelEvent>(
        [this](const StartLevelEvent &event) { HandleStartLevelEvent(event); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PreResetLevelEvent>(
        [this](const PreResetLevelEvent &) { BridgeLuaEvent(PreResetLevelEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PostResetLevelEvent>(
        [this](const PostResetLevelEvent &) { BridgeLuaEvent(PostResetLevelEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PauseLevelEvent>(
        [this](const PauseLevelEvent &) { BridgeLuaEvent(PauseLevelEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<UnpauseLevelEvent>(
        [this](const UnpauseLevelEvent &) { BridgeLuaEvent(UnpauseLevelEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PreExitLevelEvent>(
        [this](const PreExitLevelEvent &) { BridgeLuaEvent(PreExitLevelEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PostExitLevelEvent>(
        [this](const PostExitLevelEvent &) {
            DestroyLevelContexts();
            BridgeLuaEvent(PostExitLevelEvent::name);
        }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PreNextLevelEvent>(
        [this](const PreNextLevelEvent &) { BridgeLuaEvent(PreNextLevelEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PostNextLevelEvent>(
        [this](const PostNextLevelEvent &) { BridgeLuaEvent(PostNextLevelEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PreEndLevelEvent>(
        [this](const PreEndLevelEvent &) { BridgeLuaEvent(PreEndLevelEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PostEndLevelEvent>(
        [this](const PostEndLevelEvent &) { BridgeLuaEvent(PostEndLevelEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<LevelFinishEvent>(
        [this](const LevelFinishEvent &) { BridgeLuaEvent(LevelFinishEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<BallOffEvent>(
        [this](const BallOffEvent &) { BridgeLuaEvent(BallOffEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<GameOverEvent>(
        [this](const GameOverEvent &) {
            DestroyLevelContexts();
            BridgeLuaEvent(GameOverEvent::name);
        }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<CounterActiveEvent>(
        [this](const CounterActiveEvent &) { BridgeLuaEvent(CounterActiveEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<CounterInactiveEvent>(
        [this](const CounterInactiveEvent &) { BridgeLuaEvent(CounterInactiveEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<BallNavActiveEvent>(
        [this](const BallNavActiveEvent &) { BridgeLuaEvent(BallNavActiveEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<BallNavInactiveEvent>(
        [this](const BallNavInactiveEvent &) { BridgeLuaEvent(BallNavInactiveEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<CamNavActiveEvent>(
        [this](const CamNavActiveEvent &) { BridgeLuaEvent(CamNavActiveEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<CamNavInactiveEvent>(
        [this](const CamNavInactiveEvent &) { BridgeLuaEvent(CamNavInactiveEvent::name); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PreCheckpointReachedEvent>(
        [this](const PreCheckpointReachedEvent &event) {
            BridgeLuaEvent(PreCheckpointReachedEvent::name, event.sector);
        }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PostCheckpointReachedEvent>(
        [this](const PostCheckpointReachedEvent &event) {
            BridgeLuaEvent(PostCheckpointReachedEvent::name, event.sector);
        }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<ExtraPointEvent>(
        [this](const ExtraPointEvent &event) {
            BridgeLuaEvent(ExtraPointEvent::name, event.points);
        }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PreSubLifeEvent>(
        [this](const PreSubLifeEvent &event) {
            BridgeLuaEvent(PreSubLifeEvent::name, event.lifeCount);
        }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PostSubLifeEvent>(
        [this](const PostSubLifeEvent &event) {
            BridgeLuaEvent(PostSubLifeEvent::name, event.lifeCount);
        }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PreLifeUpEvent>(
        [this](const PreLifeUpEvent &event) {
            BridgeLuaEvent(PreLifeUpEvent::name, event.lifeCount);
        }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PostLifeUpEvent>(
        [this](const PostLifeUpEvent &event) {
            BridgeLuaEvent(PostLifeUpEvent::name, event.lifeCount);
        }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<PlaybackCompletedEvent>(
        [this](const PlaybackCompletedEvent &event) { HandlePlaybackCompletedEvent(event); }));
    m_EventSubscriptions.push_back(m_EventBus->Subscribe<TranslationCompletedEvent>(
        [this](const TranslationCompletedEvent &event) { HandleTranslationCompletedEvent(event); }));
}

void TASEngine::EnsureGlobalContext() {
    if (!m_ScriptContextManager) {
        return;
    }

    if (!m_ScriptContextManager->GetOrCreateGlobalContext()) {
        Log::Error("Failed to create global script context.");
    }
}

void TASEngine::EnsureLevelContext() {
    if (!m_ScriptContextManager) {
        return;
    }

    const std::string levelName = GetCurrentLevelName();
    if (levelName.empty()) {
        return;
    }

    auto context = m_ScriptContextManager->GetOrCreateLevelContext(levelName);
    if (!context) {
        Log::Error("Failed to create level script context for '%s'.", levelName.c_str());
        return;
    }

    const std::string &contextName = context->GetName();
    m_ScriptContextManager->SubscribeToEvent(contextName, StartLevelEvent::name);
    m_ScriptContextManager->SubscribeToEvent(contextName, LevelFinishEvent::name);
    m_ScriptContextManager->SubscribeToEvent(contextName, GameOverEvent::name);
    m_ScriptContextManager->SubscribeToEvent(contextName, PreCheckpointReachedEvent::name);
    m_ScriptContextManager->SubscribeToEvent(contextName, PostCheckpointReachedEvent::name);
}

void TASEngine::DestroyLevelContexts() {
    if (m_ScriptContextManager) {
        m_ScriptContextManager->DestroyAllLevelContexts();
    }
}

void TASEngine::BridgeLuaEvent(const std::string &eventName, std::optional<int> eventData) {
    if (m_ShuttingDown || eventName.empty()) {
        return;
    }

    if (m_ScriptContextManager) {
        if (eventData.has_value()) {
            m_ScriptContextManager->FireGameEventToAll(eventName, *eventData);
        } else {
            m_ScriptContextManager->FireGameEventToAll(eventName);
        }
    }

    if ((IsRecording() || IsTranslating()) && m_Recorder) {
        m_Recorder->OnGameEvent(m_CurrentTick, eventName, eventData.value_or(0));
    }
}

void TASEngine::HandleStartLevelEvent(const StartLevelEvent &) {
    EnsureLevelContext();

    const bool transitioned = m_StateMachine && m_StateMachine->IsPending()
        ? TransitionState(TASStateMachine::Event::LevelStart, "level start")
        : false;

    if (transitioned && IsPlayingScript() && IsValidationEnabled()) {
        TASProject *project = m_PlaybackService ? m_PlaybackService->GetCurrentProject() : nullptr;
        const std::string outputPath = BuildValidationOutputPath(project);
        if (!outputPath.empty() && m_ValidationService && !m_ValidationService->IsActive()) {
            StartValidationRecording(outputPath);
        }
    }

    BridgeLuaEvent(StartLevelEvent::name);
}

void TASEngine::HandlePlaybackCompletedEvent(const PlaybackCompletedEvent &event) {
    if (!m_StateMachine) {
        return;
    }

    const PlaybackType completedType = static_cast<PlaybackType>(event.playbackType);
    if (completedType != GetPlaybackType()) {
        return;
    }
    if (!m_StateMachine->IsPlaying() && !m_StateMachine->IsPaused()) {
        return;
    }

    m_ClearProjectOnStop = false;
    TransitionState(TASStateMachine::Event::Stop, "playback completed");
}

void TASEngine::HandleTranslationCompletedEvent(const TranslationCompletedEvent &) {
    if (!m_StateMachine || !m_StateMachine->IsTranslating()) {
        return;
    }

    m_ClearProjectOnStop = false;
    TransitionState(TASStateMachine::Event::Stop, "translation completed");
}

bool TASEngine::TransitionState(TASStateMachine::Event event, const char *reason) {
    if (!m_StateMachine) {
        Log::Error("State machine not available for %s.", reason ? reason : "transition");
        return false;
    }

    auto result = m_StateMachine->Transition(event);
    if (!result.IsOk()) {
        Log::Error("State transition failed for %s: %s",
                   reason ? reason : "transition",
                   result.GetError().message.c_str());
        return false;
    }

    return true;
}

std::string TASEngine::GetCurrentLevelName() const {
    if (!m_GameInterface) {
        return {};
    }

    const std::string &mapName = m_GameInterface->GetMapName();
    if (!mapName.empty()) {
        return mapName;
    }

    const int currentLevel = m_GameInterface->GetCurrentLevel();
    if (currentLevel <= 0) {
        return {};
    }

    return "Level_" + std::to_string(currentLevel);
}

std::string TASEngine::BuildValidationOutputPath(TASProject *project) const {
    if (!project) {
        return {};
    }

    fs::path path(project->GetPath());
    if (path.has_extension()) {
        path = path.parent_path();
    }

    if (path.empty()) {
        return {};
    }

    std::string outputPath = path.string();
    if (!outputPath.empty() && outputPath.back() != '\\' && outputPath.back() != '/') {
        outputPath.push_back('\\');
    }
    return outputPath;
}

void TASEngine::ClearControlRequests() {
    m_RequestedProject = nullptr;
    m_RequestedPlaybackType = PlaybackType::None;
    m_RequestedValidationRecording = false;
    m_ClearProjectOnStop = false;
}

void TASEngine::AddTimer(size_t tick, const std::function<void()> &callback) {
    if (m_GameInterface) {
        m_GameInterface->AddTimer(tick, callback);
    }
}

lua_State *TASEngine::GetLuaState() const {
    if (!m_ScriptContextManager) {
        return nullptr;
    }

    auto ctx = m_ScriptContextManager->GetContext("global");
    return ctx ? ctx->GetLuaState().lua_state() : nullptr;
}

LuaScheduler *TASEngine::GetScheduler() const {
    if (!m_ScriptContextManager) {
        return nullptr;
    }

    auto ctx = m_ScriptContextManager->GetContext("global");
    return ctx ? ctx->GetScheduler() : nullptr;
}

size_t TASEngine::GetCurrentTick() const {
    return m_CurrentTick;
}

void TASEngine::SetCurrentTick(size_t tick) {
    m_CurrentTick = tick;
}

ServiceProvider *TASEngine::GetServiceProvider() const {
    if (!m_ServiceProvider && m_ServiceContainer) {
        m_ServiceProvider = std::make_unique<ServiceProvider>(*m_ServiceContainer);
    }
    return m_ServiceProvider.get();
}

ProjectManager *TASEngine::GetProjectManager() const { return m_ProjectManager; }
InputSystem *TASEngine::GetInputSystem() const { return m_InputSystem; }
EventManager *TASEngine::GetEventManager() const { return m_EventManager; }
ScriptContextManager *TASEngine::GetScriptContextManager() const { return m_ScriptContextManager; }
#ifdef ENABLE_REPL
LuaREPLServer *TASEngine::GetREPLServer() const { return m_REPLServer; }
#endif
RecordPlayer *TASEngine::GetRecordPlayer() const { return m_RecordPlayer; }
Recorder *TASEngine::GetRecorder() const { return m_Recorder; }
ScriptGenerator *TASEngine::GetScriptGenerator() const { return m_ScriptGenerator; }
StartupProjectManager *TASEngine::GetStartupProjectManager() const { return m_StartupProjectManager; }
TASStateMachine *TASEngine::GetStateMachine() const { return m_StateMachine; }
