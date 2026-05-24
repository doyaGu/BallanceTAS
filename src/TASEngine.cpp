#include "TASEngine.h"

#include <filesystem>

#include "EngineBootstrap.h"
#include "ContextLifecycleCoordinator.h"
#include "EventManager.h"
#include "GameEvents.h"
#include "GameInterface.h"
#include "HookManager.h"
#include "InputSystem.h"
#include "Logger.h"
#include "LuaTypedEventBridge.h"
#include "PlaybackService.h"
#include "ProjectManager.h"
#include "RecordPlayer.h"
#include "Recorder.h"
#include "RecordingService.h"
#include "RuntimeEventRouter.h"
#include "SavestateManager.h"
#include "ScriptContext.h"
#include "ScriptContextManager.h"
#include "ScriptGenerator.h"
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

    if (!EngineBootstrap::InitializeHighLevelSubsystems(*this)) {
        return false;
    }

    Log::Info("TASEngine initialization complete.");
    return true;
}

void TASEngine::Shutdown() {
    const bool wasShuttingDown = m_ShuttingDown.exchange(true);
    if (wasShuttingDown) {
        return;
    }

    auto *stateMachine = m_ServiceContainer.Resolve<TASStateMachine>();

    bool handledByStateMachine = false;
    if (stateMachine && !stateMachine->IsShuttingDown()) {
        auto result = stateMachine->Transition(TASStateMachine::Event::Shutdown);
        if (!result.IsOk()) {
            Log::Error("Failed to transition to shutdown state: %s",
                       result.GetError().message.c_str());
        } else {
            handledByStateMachine = true;
        }
    }

    if (!handledByStateMachine) {
        auto *validationService = m_ServiceContainer.Resolve<ValidationService>();
        auto *playbackService = m_ServiceContainer.Resolve<PlaybackService>();
        auto *recordingService = m_ServiceContainer.Resolve<RecordingService>();
        auto *translationService = m_ServiceContainer.Resolve<TranslationService>();

        if (validationService && validationService->IsActive()) {
            validationService->StopImmediate();
        }
        if (playbackService) {
            playbackService->StopPlaybackImmediate();
        }
        if (recordingService) {
            recordingService->StopRecordingImmediate();
        }
        if (translationService) {
            translationService->StopTranslationImmediate();
        }
    }

    auto *router = m_ServiceContainer.Resolve<RuntimeEventRouter>();
    if (router) {
        router->Shutdown();
    }
    auto *bridge = m_ServiceContainer.Resolve<LuaTypedEventBridge>();
    if (bridge) {
        bridge->Shutdown();
    }

#ifdef ENABLE_REPL
    auto *replServer = m_ServiceContainer.Resolve<LuaREPLServer>();
    if (replServer) {
        replServer->Shutdown();
    }
#endif

    auto *scriptContextManager = m_ServiceContainer.Resolve<ScriptContextManager>();
    if (scriptContextManager) {
        scriptContextManager->Shutdown();
    }

    m_ServiceContainer.Clear();

    m_GameInterface = nullptr;
    Log::Info("TASEngine shutdown complete.");
}

void TASEngine::Start() {
    if (m_ShuttingDown || !m_GameInterface) {
        return;
    }

    const float deterministicDeltaTime = ResolveLevelLoadPhysicsDeltaTime();
    AddTimer(1ul, [this, deterministicDeltaTime]() {
        if (m_GameInterface) {
            if (deterministicDeltaTime > 0.0f) {
                Log::Info("Resetting physics time with TAS delta %.6f ms.", deterministicDeltaTime);
                m_GameInterface->ResetPhysicsTime(deterministicDeltaTime);
            } else {
                m_GameInterface->ResetPhysicsTime();
            }
        }
    });
}

void TASEngine::Stop() {
    auto *stateMachine = m_ServiceContainer.Resolve<TASStateMachine>();
    if (m_ShuttingDown || !stateMachine || stateMachine->IsIdle() || stateMachine->IsShuttingDown()) {
        return;
    }

    bool shouldKeepGlobalScript = false;
    auto *playbackService = m_ServiceContainer.Resolve<PlaybackService>();
    if (IsPlayingScript() && playbackService) {
        auto *project = playbackService->GetCurrentProject();
        shouldKeepGlobalScript = project && project->IsGlobalProject();
    }

    if (shouldKeepGlobalScript) {
        auto *validationService = m_ServiceContainer.Resolve<ValidationService>();
        if (validationService && validationService->IsActive()) {
            StopValidationRecording();
        }
        Log::Info("Keeping global script playback active during level transition.");
        return;
    }

    m_Requests.clearProjectOnStop = true;
    const bool transitioned = TransitionState(TASStateMachine::Event::LevelEnd, "level end");
    if (transitioned && IsAutoRestartEnabled()) {
        RestartCurrentProject();
    }
}

bool TASEngine::StartRecording() {
    auto *stateMachine = m_ServiceContainer.Resolve<TASStateMachine>();
    if (m_ShuttingDown || !stateMachine || !stateMachine->IsIdle()) {
        Log::Warn("Cannot start recording: TAS is not idle.");
        return false;
    }

    ClearControlRequests();
    m_LastCompletedPlaybackProjectName.clear();
    m_Requests.requestedValidationRecording = false;
    return TransitionState(TASStateMachine::Event::StartRecording, "start recording");
}

void TASEngine::StopRecording() {
    auto *recordingService = m_ServiceContainer.Resolve<RecordingService>();
    if (m_ShuttingDown) {
        if (recordingService) {
            recordingService->StopRecordingImmediate();
        }
        return;
    }

    auto *stateMachine = m_ServiceContainer.Resolve<TASStateMachine>();
    if (!stateMachine || (!stateMachine->IsRecording() && !stateMachine->IsPendingRecord())) {
        return;
    }

    m_Requests.clearProjectOnStop = false;
    TransitionState(TASStateMachine::Event::Stop, "stop recording");
}

size_t TASEngine::GetRecordingFrameCount() const {
    auto *recordingService = m_ServiceContainer.Resolve<RecordingService>();
    return recordingService ? recordingService->GetFrameCount() : 0;
}

bool TASEngine::StartReplay() {
    auto *stateMachine = m_ServiceContainer.Resolve<TASStateMachine>();
    if (m_ShuttingDown || !stateMachine || !stateMachine->IsIdle()) {
        Log::Warn("Cannot start replay: TAS is not idle.");
        return false;
    }

    auto *projectManager = m_ServiceContainer.Resolve<ProjectManager>();
    if (!projectManager) {
        Log::Error("ProjectManager not available.");
        return false;
    }

    TASProject *project = projectManager->GetCurrentProject();
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
    m_LastCompletedPlaybackProjectName.clear();
    m_Requests.requestedProject = project;
    m_Requests.requestedPlaybackType = type;

    return TransitionState(
        type == PlaybackType::Script
            ? TASStateMachine::Event::StartScriptPlayback
            : TASStateMachine::Event::StartRecordPlayback,
        "start replay"
    );
}

void TASEngine::StopReplay(bool clearProject) {
    auto *playbackService = m_ServiceContainer.Resolve<PlaybackService>();
    if (m_ShuttingDown) {
        auto *validationService = m_ServiceContainer.Resolve<ValidationService>();
        if (validationService && validationService->IsActive()) {
            validationService->StopImmediate();
        }
        if (playbackService) {
            playbackService->StopPlaybackImmediate();
        }
        return;
    }

    auto *stateMachine = m_ServiceContainer.Resolve<TASStateMachine>();
    if (!stateMachine ||
        (!stateMachine->IsPlaying() && !stateMachine->IsPaused() && !stateMachine->IsPendingPlay())) {
        return;
    }

    m_Requests.clearProjectOnStop = clearProject;
    m_LastCompletedPlaybackProjectName.clear();
    TransitionState(TASStateMachine::Event::Stop, "stop replay");
}

bool TASEngine::StartTranslation() {
    auto *stateMachine = m_ServiceContainer.Resolve<TASStateMachine>();
    if (m_ShuttingDown || !stateMachine || !stateMachine->IsIdle()) {
        Log::Warn("Cannot start translation: TAS is not idle.");
        return false;
    }

    auto *projectManager = m_ServiceContainer.Resolve<ProjectManager>();
    if (!projectManager) {
        Log::Error("ProjectManager not available.");
        return false;
    }

    TASProject *project = projectManager->GetCurrentProject();
    if (!project) {
        Log::Error("No project selected for translation.");
        return false;
    }

    ClearControlRequests();
    m_LastCompletedPlaybackProjectName.clear();
    m_Requests.requestedProject = project;

    return TransitionState(TASStateMachine::Event::StartTranslation, "start translation");
}

void TASEngine::StopTranslation(bool clearProject) {
    auto *translationService = m_ServiceContainer.Resolve<TranslationService>();
    if (m_ShuttingDown) {
        if (translationService) {
            translationService->StopTranslationImmediate();
        }
        return;
    }

    auto *stateMachine = m_ServiceContainer.Resolve<TASStateMachine>();
    if (!stateMachine ||
        (!stateMachine->IsTranslating() && !stateMachine->IsPendingTranslate())) {
        return;
    }

    m_Requests.clearProjectOnStop = clearProject;
    TransitionState(TASStateMachine::Event::Stop, "stop translation");
}

bool TASEngine::StartValidationRecording(const std::string &outputPath) {
    auto *validationService = m_ServiceContainer.Resolve<ValidationService>();
    auto *playbackService = m_ServiceContainer.Resolve<PlaybackService>();
    if (!validationService || !playbackService) {
        return false;
    }

    auto result = validationService->Start(outputPath, *playbackService);
    if (!result.IsOk()) {
        Log::Error("Validation recording: %s", result.GetError().message.c_str());
        return false;
    }
    return true;
}

bool TASEngine::StopValidationRecording() {
    auto *validationService = m_ServiceContainer.Resolve<ValidationService>();
    if (!validationService || !validationService->IsActive()) {
        return false;
    }

    auto result = validationService->Stop();
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
    auto *validationService = m_ServiceContainer.Resolve<ValidationService>();
    return validationService ? validationService->GetOutputPath() : empty;
}

std::string TASEngine::GetLastTranslationResultMessage() const {
    auto *translationService = m_ServiceContainer.Resolve<TranslationService>();
    return translationService ? translationService->GetLastResultMessage() : std::string{};
}

bool TASEngine::RestartCurrentProject() {
    auto *projectManager = m_ServiceContainer.Resolve<ProjectManager>();
    if (m_ShuttingDown || !projectManager) {
        Log::Warn("Cannot restart during shutdown or without ProjectManager.");
        return false;
    }

    TASProject *project = projectManager->GetCurrentProject();
    if (!project || !project->IsValid()) {
        Log::Error("No valid project to restart.");
        return false;
    }

    Log::Info("Restarting TAS project: %s", project->GetName().c_str());
    return StartReplay();
}

bool TASEngine::IsPlaying() const {
    auto *sm = m_ServiceContainer.Resolve<TASStateMachine>();
    return sm && sm->IsPlaying();
}

bool TASEngine::IsRecording() const {
    auto *sm = m_ServiceContainer.Resolve<TASStateMachine>();
    return sm && sm->IsRecording();
}

bool TASEngine::IsTranslating() const {
    auto *sm = m_ServiceContainer.Resolve<TASStateMachine>();
    return sm && sm->IsTranslating();
}

bool TASEngine::IsIdle() const {
    auto *sm = m_ServiceContainer.Resolve<TASStateMachine>();
    return sm && sm->IsIdle();
}

bool TASEngine::IsPaused() const {
    auto *sm = m_ServiceContainer.Resolve<TASStateMachine>();
    return sm && sm->IsPaused();
}

bool TASEngine::IsPlayingScript() const {
    return (IsPlaying() || IsPaused()) && GetPlaybackType() == PlaybackType::Script;
}

bool TASEngine::IsPlayingRecord() const {
    return (IsPlaying() || IsPaused()) && GetPlaybackType() == PlaybackType::Record;
}

PlaybackType TASEngine::GetPlaybackType() const {
    auto *sm = m_ServiceContainer.Resolve<TASStateMachine>();
    if (!sm) {
        return PlaybackType::None;
    }

    switch (sm->GetCurrentState()) {
    case TASStateMachine::State::PendingScriptPlayback:
    case TASStateMachine::State::PlayingScript:
        return PlaybackType::Script;
    case TASStateMachine::State::PendingRecordPlayback:
    case TASStateMachine::State::PlayingRecord:
        return PlaybackType::Record;
    case TASStateMachine::State::Paused:
        switch (sm->GetPausedFromState()) {
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

    auto *playbackService = m_ServiceContainer.Resolve<PlaybackService>();
    return playbackService ? playbackService->GetPlaybackType() : PlaybackType::None;
}

bool TASEngine::IsPendingPlay() const {
    auto *sm = m_ServiceContainer.Resolve<TASStateMachine>();
    return sm && sm->IsPendingPlay();
}

bool TASEngine::IsPendingRecord() const {
    auto *sm = m_ServiceContainer.Resolve<TASStateMachine>();
    return sm && sm->IsPendingRecord();
}

bool TASEngine::IsPendingTranslate() const {
    auto *sm = m_ServiceContainer.Resolve<TASStateMachine>();
    return sm && sm->IsPendingTranslate();
}

bool TASEngine::TransitionState(TASStateMachine::Event event, const char *reason) {
    auto *sm = m_ServiceContainer.Resolve<TASStateMachine>();
    if (!sm) {
        Log::Error("State machine not available for %s.", reason ? reason : "transition");
        return false;
    }

    auto result = sm->Transition(event);
    if (!result.IsOk()) {
        Log::Error("State transition failed for %s: %s",
                   reason ? reason : "transition",
                   result.GetError().message.c_str());
        return false;
    }

    return true;
}

void TASEngine::HandlePlaybackCompleted() {
    auto *sm = m_ServiceContainer.Resolve<TASStateMachine>();
    if (m_ShuttingDown || !sm || (!sm->IsPlaying() && !sm->IsPaused())) {
        return;
    }

    if (auto *playbackService = m_ServiceContainer.Resolve<PlaybackService>()) {
        if (auto *project = playbackService->GetCurrentProject()) {
            m_LastCompletedPlaybackProjectName = project->GetName();
        }
    }

    m_Requests.clearProjectOnStop = false;
    TransitionState(TASStateMachine::Event::Stop, "playback completed");
}

float TASEngine::ResolveLevelLoadPhysicsDeltaTime() const {
    const auto *stateMachine = m_ServiceContainer.Resolve<TASStateMachine>();
    if (!stateMachine || stateMachine->IsIdle() || stateMachine->IsShuttingDown()) {
        return 0.0f;
    }

    if (m_Requests.requestedProject && m_Requests.requestedProject->IsValid()) {
        return m_Requests.requestedProject->GetDeltaTime();
    }

    if (auto *playbackService = m_ServiceContainer.Resolve<PlaybackService>()) {
        if (auto *project = playbackService->GetCurrentProject();
            project && project->IsValid()) {
            return project->GetDeltaTime();
        }
    }

    if (stateMachine->IsPendingRecord() || stateMachine->IsRecording()) {
        const auto *recorder = m_ServiceContainer.Resolve<Recorder>();
        return recorder ? recorder->GetDeltaTime() : 0.0f;
    }

    if (stateMachine->IsPendingTranslate() || stateMachine->IsTranslating()) {
        const auto *recorder = m_ServiceContainer.Resolve<Recorder>();
        return recorder ? recorder->GetDeltaTime() : 0.0f;
    }

    return 0.0f;
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
    m_Requests.Clear();
}

void TASEngine::AddTimer(size_t tick, const std::function<void()> &callback) {
    if (m_GameInterface) {
        m_GameInterface->AddTimer(tick, callback);
    }
}

lua_State *TASEngine::GetLuaState() const {
    auto *scriptContextManager = m_ServiceContainer.Resolve<ScriptContextManager>();
    if (!scriptContextManager) {
        return nullptr;
    }

    auto ctx = scriptContextManager->GetContext("global");
    return ctx ? ctx->GetLuaState().Get() : nullptr;
}

LuaScheduler *TASEngine::GetScheduler() const {
    auto *scriptContextManager = m_ServiceContainer.Resolve<ScriptContextManager>();
    if (!scriptContextManager) {
        return nullptr;
    }

    auto ctx = scriptContextManager->GetContext("global");
    return ctx ? ctx->GetScheduler() : nullptr;
}

size_t TASEngine::GetCurrentTick() const {
    if (auto *playbackService = m_ServiceContainer.Resolve<PlaybackService>()) {
        if (playbackService->IsPlaying() || playbackService->IsPaused()) {
            return playbackService->GetCurrentTick();
        }
    }

    if (auto *recordingService = m_ServiceContainer.Resolve<RecordingService>()) {
        if (recordingService->IsRecording()) {
            return recordingService->GetCurrentTick();
        }
    }

    if (auto *translationService = m_ServiceContainer.Resolve<TranslationService>()) {
        if (translationService->IsTranslating()) {
            return translationService->GetCurrentTick();
        }
    }

    return m_CurrentTick;
}

void TASEngine::SetCurrentTick(size_t tick) {
    m_CurrentTick = tick;
}
