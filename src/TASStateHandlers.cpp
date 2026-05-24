#include "TASStateHandlers.h"

#include <stdexcept>

#include "GameInterface.h"
#include "InputSystem.h"
#include "Logger.h"
#include "PlaybackService.h"
#include "ProjectManager.h"
#include "Recorder.h"
#include "RecordPlayer.h"
#include "RecordingService.h"
#include "ScriptContextManager.h"
#include "TASEngine.h"
#include "TranslationService.h"
#include "UIManager.h"
#include "ValidationService.h"

BaseTASStateHandler::BaseTASStateHandler(TASEngine *engine)
    : m_Engine(engine) {
    if (!m_Engine) {
        throw std::invalid_argument("TASEngine cannot be null");
    }
}

ServiceProvider &BaseTASStateHandler::Services() const {
    return m_Engine->GetServiceProvider();
}

Recorder *BaseTASStateHandler::GetRecorder() const {
    return Services().Resolve<Recorder>();
}

RecordPlayer *BaseTASStateHandler::GetRecordPlayer() const {
    return Services().Resolve<RecordPlayer>();
}

ScriptContextManager *BaseTASStateHandler::GetScriptContextManager() const {
    return Services().Resolve<ScriptContextManager>();
}

InputSystem *BaseTASStateHandler::GetInputSystem() const {
    return Services().Resolve<InputSystem>();
}

GameInterface *BaseTASStateHandler::GetGameInterface() const {
    return Services().Resolve<GameInterface>();
}

ProjectManager *BaseTASStateHandler::GetProjectManager() const {
    return Services().Resolve<ProjectManager>();
}

RecordingService *BaseTASStateHandler::GetRecordingService() const {
    return Services().Resolve<RecordingService>();
}

PlaybackService *BaseTASStateHandler::GetPlaybackService() const {
    return Services().Resolve<PlaybackService>();
}

TranslationService *BaseTASStateHandler::GetTranslationService() const {
    return Services().Resolve<TranslationService>();
}

ValidationService *BaseTASStateHandler::GetValidationService() const {
    return Services().Resolve<ValidationService>();
}

void BaseTASStateHandler::SetUIMode(UIMode mode) const {
    if (auto *game = GetGameInterface()) {
        game->SetUIMode(mode);
    }
}

void BaseTASStateHandler::ResetInputSystem() const {
    if (auto *input = GetInputSystem()) {
        input->Reset();
        input->SetEnabled(false);
    }
}

void BaseTASStateHandler::StopValidationImmediate() const {
    if (auto *validation = GetValidationService();
        validation && validation->IsActive()) {
        validation->StopImmediate();
    }
}

void BaseTASStateHandler::StopValidationGraceful() const {
    if (auto *validation = GetValidationService();
        validation && validation->IsActive()) {
        auto result = validation->Stop();
        if (!result.IsOk()) {
            Log::Error("Failed to stop validation recording: %s",
                       result.GetError().message.c_str());
        }
    }
}

IdleHandler::IdleHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> IdleHandler::OnEnter(TASStateMachine::State previousState) {
    Log::Info("Entering Idle state from %s",
              TASStateMachine::StateToString(previousState));

    ResetInputSystem();
    SetUIMode(UIMode::Idle);
    m_Engine->ClearControlRequests();
    return Result<void>::Ok();
}

Result<void> IdleHandler::OnExit(TASStateMachine::State nextState) {
    Log::Info("Exiting Idle state to %s",
              TASStateMachine::StateToString(nextState));
    return Result<void>::Ok();
}

void IdleHandler::OnTick() {
}

bool IdleHandler::CanTransitionTo(TASStateMachine::State newState) const {
    return newState == TASStateMachine::State::PendingRecord ||
           newState == TASStateMachine::State::PendingScriptPlayback ||
           newState == TASStateMachine::State::PendingRecordPlayback ||
           newState == TASStateMachine::State::PendingTranslation ||
           newState == TASStateMachine::State::ShuttingDown;
}

PendingRecordHandler::PendingRecordHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> PendingRecordHandler::OnEnter(TASStateMachine::State) {
    auto *service = GetRecordingService();
    if (!service) {
        return Result<void>::Error("RecordingService not available", "state_handler");
    }

    auto result = service->PrepareRecording(m_Engine->ShouldUseValidationForRecording());
    if (!result.IsOk()) {
        return result;
    }

    SetUIMode(UIMode::Idle);
    return Result<void>::Ok();
}

Result<void> PendingRecordHandler::OnExit(TASStateMachine::State nextState) {
    if (nextState == TASStateMachine::State::Recording) {
        return Result<void>::Ok();
    }

    auto *service = GetRecordingService();
    if (!service || !service->IsPrepared()) {
        return Result<void>::Ok();
    }

    auto result = service->StopRecordingGraceful();
    return result.IsOk()
        ? Result<void>::Ok()
        : Result<void>::Error(result.GetError());
}

void PendingRecordHandler::OnTick() {
}

bool PendingRecordHandler::CanTransitionTo(TASStateMachine::State newState) const {
    return newState == TASStateMachine::State::Recording ||
           newState == TASStateMachine::State::Idle ||
           newState == TASStateMachine::State::ShuttingDown;
}

RecordingHandler::RecordingHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> RecordingHandler::OnEnter(TASStateMachine::State) {
    auto *service = GetRecordingService();
    if (!service) {
        return Result<void>::Error("RecordingService not available", "state_handler");
    }

    auto result = service->ActivateRecording();
    if (!result.IsOk()) {
        return result;
    }

    SetUIMode(UIMode::Recording);
    return Result<void>::Ok();
}

Result<void> RecordingHandler::OnExit(TASStateMachine::State nextState) {
    auto *service = GetRecordingService();
    if (!service) {
        return Result<void>::Ok();
    }

    if (nextState == TASStateMachine::State::ShuttingDown) {
        service->StopRecordingImmediate();
        return Result<void>::Ok();
    }

    if (!service->IsRecording()) {
        return Result<void>::Ok();
    }

    auto result = service->StopRecordingGraceful();
    return result.IsOk()
        ? Result<void>::Ok()
        : Result<void>::Error(result.GetError());
}

void RecordingHandler::OnTick() {
}

bool RecordingHandler::CanTransitionTo(TASStateMachine::State newState) const {
    return newState == TASStateMachine::State::Idle ||
           newState == TASStateMachine::State::ShuttingDown;
}

PendingScriptPlaybackHandler::PendingScriptPlaybackHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> PendingScriptPlaybackHandler::OnEnter(TASStateMachine::State) {
    auto *service = GetPlaybackService();
    auto *project = m_Engine->GetRequestedProject();
    if (!service) {
        return Result<void>::Error("PlaybackService not available", "state_handler");
    }

    auto result = service->PreparePlayback(project, PlaybackType::Script);
    if (!result.IsOk()) {
        return result;
    }

    SetUIMode(UIMode::Idle);
    return Result<void>::Ok();
}

Result<void> PendingScriptPlaybackHandler::OnExit(TASStateMachine::State nextState) {
    if (nextState == TASStateMachine::State::PlayingScript) {
        return Result<void>::Ok();
    }

    auto *service = GetPlaybackService();
    if (!service || !service->IsPrepared()) {
        return Result<void>::Ok();
    }

    return service->StopPlaybackGraceful(m_Engine->ShouldClearProjectOnStop());
}

void PendingScriptPlaybackHandler::OnTick() {
}

bool PendingScriptPlaybackHandler::CanTransitionTo(TASStateMachine::State newState) const {
    return newState == TASStateMachine::State::PlayingScript ||
           newState == TASStateMachine::State::Idle ||
           newState == TASStateMachine::State::ShuttingDown;
}

PendingRecordPlaybackHandler::PendingRecordPlaybackHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> PendingRecordPlaybackHandler::OnEnter(TASStateMachine::State) {
    auto *service = GetPlaybackService();
    auto *project = m_Engine->GetRequestedProject();
    if (!service) {
        return Result<void>::Error("PlaybackService not available", "state_handler");
    }

    auto result = service->PreparePlayback(project, PlaybackType::Record);
    if (!result.IsOk()) {
        return result;
    }

    SetUIMode(UIMode::Idle);
    return Result<void>::Ok();
}

Result<void> PendingRecordPlaybackHandler::OnExit(TASStateMachine::State nextState) {
    if (nextState == TASStateMachine::State::PlayingRecord) {
        return Result<void>::Ok();
    }

    auto *service = GetPlaybackService();
    if (!service || !service->IsPrepared()) {
        return Result<void>::Ok();
    }

    return service->StopPlaybackGraceful(m_Engine->ShouldClearProjectOnStop());
}

void PendingRecordPlaybackHandler::OnTick() {
}

bool PendingRecordPlaybackHandler::CanTransitionTo(TASStateMachine::State newState) const {
    return newState == TASStateMachine::State::PlayingRecord ||
           newState == TASStateMachine::State::Idle ||
           newState == TASStateMachine::State::ShuttingDown;
}

PlayingScriptHandler::PlayingScriptHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> PlayingScriptHandler::OnEnter(TASStateMachine::State previousState) {
    auto *service = GetPlaybackService();
    if (!service) {
        return Result<void>::Error("PlaybackService not available", "state_handler");
    }

    if (previousState == TASStateMachine::State::Paused) {
        service->Resume();
    } else {
        auto result = service->ActivatePlayback();
        if (!result.IsOk()) {
            return result;
        }
    }

    SetUIMode(UIMode::Playing);
    return Result<void>::Ok();
}

Result<void> PlayingScriptHandler::OnExit(TASStateMachine::State nextState) {
    auto *service = GetPlaybackService();
    if (!service) {
        return Result<void>::Ok();
    }

    if (nextState == TASStateMachine::State::Paused) {
        service->Pause();
        return Result<void>::Ok();
    }

    if (nextState == TASStateMachine::State::ShuttingDown) {
        StopValidationImmediate();
        service->StopPlaybackImmediate();
        return Result<void>::Ok();
    }

    StopValidationGraceful();
    if (!service->IsPlaying() && !service->IsPaused()) {
        return Result<void>::Ok();
    }

    return service->StopPlaybackGraceful(m_Engine->ShouldClearProjectOnStop());
}

void PlayingScriptHandler::OnTick() {
}

bool PlayingScriptHandler::CanTransitionTo(TASStateMachine::State newState) const {
    return newState == TASStateMachine::State::Paused ||
           newState == TASStateMachine::State::Idle ||
           newState == TASStateMachine::State::ShuttingDown;
}

PlayingRecordHandler::PlayingRecordHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> PlayingRecordHandler::OnEnter(TASStateMachine::State previousState) {
    auto *service = GetPlaybackService();
    if (!service) {
        return Result<void>::Error("PlaybackService not available", "state_handler");
    }

    if (previousState == TASStateMachine::State::Paused) {
        service->Resume();
    } else {
        auto result = service->ActivatePlayback();
        if (!result.IsOk()) {
            return result;
        }
    }

    SetUIMode(UIMode::Playing);
    return Result<void>::Ok();
}

Result<void> PlayingRecordHandler::OnExit(TASStateMachine::State nextState) {
    auto *service = GetPlaybackService();
    if (!service) {
        return Result<void>::Ok();
    }

    if (nextState == TASStateMachine::State::Paused) {
        service->Pause();
        return Result<void>::Ok();
    }

    if (nextState == TASStateMachine::State::ShuttingDown) {
        service->StopPlaybackImmediate();
        return Result<void>::Ok();
    }

    if (!service->IsPlaying() && !service->IsPaused()) {
        return Result<void>::Ok();
    }

    return service->StopPlaybackGraceful(m_Engine->ShouldClearProjectOnStop());
}

void PlayingRecordHandler::OnTick() {
}

bool PlayingRecordHandler::CanTransitionTo(TASStateMachine::State newState) const {
    return newState == TASStateMachine::State::Paused ||
           newState == TASStateMachine::State::Idle ||
           newState == TASStateMachine::State::ShuttingDown;
}

PendingTranslationHandler::PendingTranslationHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> PendingTranslationHandler::OnEnter(TASStateMachine::State) {
    auto *service = GetTranslationService();
    auto *project = m_Engine->GetRequestedProject();
    if (!service) {
        return Result<void>::Error("TranslationService not available", "state_handler");
    }

    auto result = service->PrepareTranslation(project);
    if (!result.IsOk()) {
        return result;
    }

    SetUIMode(UIMode::Idle);
    return Result<void>::Ok();
}

Result<void> PendingTranslationHandler::OnExit(TASStateMachine::State nextState) {
    if (nextState == TASStateMachine::State::Translating) {
        return Result<void>::Ok();
    }

    auto *service = GetTranslationService();
    if (!service || !service->IsPrepared()) {
        return Result<void>::Ok();
    }

    return service->StopTranslationGraceful(m_Engine->ShouldClearProjectOnStop());
}

void PendingTranslationHandler::OnTick() {
}

bool PendingTranslationHandler::CanTransitionTo(TASStateMachine::State newState) const {
    return newState == TASStateMachine::State::Translating ||
           newState == TASStateMachine::State::Idle ||
           newState == TASStateMachine::State::ShuttingDown;
}

TranslatingHandler::TranslatingHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> TranslatingHandler::OnEnter(TASStateMachine::State) {
    auto *service = GetTranslationService();
    if (!service) {
        return Result<void>::Error("TranslationService not available", "state_handler");
    }

    auto result = service->ActivateTranslation();
    if (!result.IsOk()) {
        return result;
    }

    SetUIMode(UIMode::Recording);
    return Result<void>::Ok();
}

Result<void> TranslatingHandler::OnExit(TASStateMachine::State nextState) {
    auto *service = GetTranslationService();
    if (!service) {
        return Result<void>::Ok();
    }

    if (nextState == TASStateMachine::State::ShuttingDown) {
        service->StopTranslationImmediate();
        return Result<void>::Ok();
    }

    if (!service->IsTranslating()) {
        return Result<void>::Ok();
    }

    return service->StopTranslationGraceful(m_Engine->ShouldClearProjectOnStop());
}

void TranslatingHandler::OnTick() {
}

bool TranslatingHandler::CanTransitionTo(TASStateMachine::State newState) const {
    return newState == TASStateMachine::State::Idle ||
           newState == TASStateMachine::State::ShuttingDown;
}

PausedHandler::PausedHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> PausedHandler::OnEnter(TASStateMachine::State previousState) {
    Log::Info("Entering Paused state from %s",
              TASStateMachine::StateToString(previousState));
    SetUIMode(UIMode::Paused);
    return Result<void>::Ok();
}

Result<void> PausedHandler::OnExit(TASStateMachine::State nextState) {
    auto *service = GetPlaybackService();
    if (!service) {
        return Result<void>::Ok();
    }

    if (nextState == TASStateMachine::State::PlayingScript ||
        nextState == TASStateMachine::State::PlayingRecord) {
        return Result<void>::Ok();
    }

    if (nextState == TASStateMachine::State::ShuttingDown) {
        StopValidationImmediate();
        service->StopPlaybackImmediate();
        return Result<void>::Ok();
    }

    StopValidationGraceful();
    return service->StopPlaybackGraceful(m_Engine->ShouldClearProjectOnStop());
}

void PausedHandler::OnTick() {
}

bool PausedHandler::CanTransitionTo(TASStateMachine::State newState) const {
    return newState == TASStateMachine::State::PlayingScript ||
           newState == TASStateMachine::State::PlayingRecord ||
           newState == TASStateMachine::State::Idle ||
           newState == TASStateMachine::State::ShuttingDown;
}

ShuttingDownHandler::ShuttingDownHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> ShuttingDownHandler::OnEnter(TASStateMachine::State previousState) {
    Log::Info("Entering ShuttingDown state from %s",
              TASStateMachine::StateToString(previousState));

    StopValidationImmediate();

    if (auto *playback = GetPlaybackService()) {
        playback->StopPlaybackImmediate();
    }
    if (auto *recording = GetRecordingService()) {
        recording->StopRecordingImmediate();
    }
    if (auto *translation = GetTranslationService()) {
        translation->StopTranslationImmediate();
    }

    ResetInputSystem();
    SetUIMode(UIMode::Idle);
    m_Engine->ClearControlRequests();
    return Result<void>::Ok();
}

Result<void> ShuttingDownHandler::OnExit(TASStateMachine::State) {
    return Result<void>::Error("Cannot leave shutting down state", "state_handler");
}

void ShuttingDownHandler::OnTick() {
}

bool ShuttingDownHandler::CanTransitionTo(TASStateMachine::State) const {
    return false;
}
