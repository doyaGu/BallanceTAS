#include "RuntimeEventRouter.h"

#include "ContextLifecycleCoordinator.h"
#include "GameEvents.h"
#include "Logger.h"
#include "OperationRequestStore.h"
#include "PlaybackService.h"
#include "PlaybackTypes.h"
#include "TASProject.h"
#include "TASStateMachine.h"
#include "TranslationService.h"
#include "ValidationService.h"

RuntimeEventRouter::RuntimeEventRouter(
    EventBus &eventBus,
    TASStateMachine &stateMachine,
    ContextLifecycleCoordinator &contextLifecycleCoordinator,
    PlaybackService *playbackService,
    TranslationService *translationService,
    ValidationService *validationService,
    OperationRequestStore &requests,
    std::function<bool()> validationEnabledProvider,
    std::function<std::string(TASProject *)> validationOutputPathBuilder)
    : m_EventBus(eventBus),
      m_StateMachine(stateMachine),
      m_ContextLifecycleCoordinator(contextLifecycleCoordinator),
      m_PlaybackService(playbackService),
      m_TranslationService(translationService),
      m_ValidationService(validationService),
      m_Requests(requests),
      m_ValidationEnabledProvider(std::move(validationEnabledProvider)),
      m_ValidationOutputPathBuilder(std::move(validationOutputPathBuilder)) {
}

void RuntimeEventRouter::Initialize() {
    m_Subscriptions.clear();

    m_Subscriptions.push_back(m_EventBus.Subscribe<PostStartMenuEvent>(
        [this](const PostStartMenuEvent &event) {
            m_ContextLifecycleCoordinator.OnPostStartMenu(event);
        }));
    m_Subscriptions.push_back(m_EventBus.Subscribe<PreLoadLevelEvent>(
        [this](const PreLoadLevelEvent &) {
            HandlePreLoadLevel();
        }));
    m_Subscriptions.push_back(m_EventBus.Subscribe<StartLevelEvent>(
        [this](const StartLevelEvent &event) {
            m_ContextLifecycleCoordinator.OnStartLevel(event);
            HandleStartLevel();
        }));
    m_Subscriptions.push_back(m_EventBus.Subscribe<PostExitLevelEvent>(
        [this](const PostExitLevelEvent &event) {
            m_ContextLifecycleCoordinator.OnPostExitLevel(event);
        }));
    m_Subscriptions.push_back(m_EventBus.Subscribe<GameOverEvent>(
        [this](const GameOverEvent &event) {
            m_ContextLifecycleCoordinator.OnGameOver(event);
        }));
    m_Subscriptions.push_back(m_EventBus.Subscribe<PlaybackCompletedEvent>(
        [this](const PlaybackCompletedEvent &event) {
            HandlePlaybackCompleted(event.playbackType);
        }));
    m_Subscriptions.push_back(m_EventBus.Subscribe<TranslationCompletedEvent>(
        [this](const TranslationCompletedEvent &) {
            HandleTranslationCompleted();
        }));
}

void RuntimeEventRouter::Shutdown() {
    m_Subscriptions.clear();
}

void RuntimeEventRouter::HandlePreLoadLevel() {
    const auto state = m_StateMachine.GetCurrentState();
    if (state != TASStateMachine::State::PendingRecord &&
        state != TASStateMachine::State::PendingScriptPlayback &&
        state != TASStateMachine::State::PendingRecordPlayback &&
        state != TASStateMachine::State::PendingTranslation) {
        return;
    }

    auto result = m_StateMachine.Transition(TASStateMachine::Event::LevelLoadStart);
    if (!result.IsOk() && result.GetError().severity != ErrorSeverity::Warning) {
        Log::Error("State transition failed for level load start: %s",
                   result.GetError().message.c_str());
    }
}

void RuntimeEventRouter::HandleStartLevel() {
    if (m_StateMachine.IsPending()) {
        auto result = m_StateMachine.Transition(TASStateMachine::Event::LevelStart);
        if (!result.IsOk() && result.GetError().severity != ErrorSeverity::Warning) {
            Log::Error("State transition failed for level start: %s",
                       result.GetError().message.c_str());
        }
    }

    if (!m_PlaybackService || !m_ValidationService) {
        return;
    }

    if (m_PlaybackService->GetPlaybackType() != PlaybackType::Script) {
        return;
    }
    if (!m_ValidationEnabledProvider || !m_ValidationEnabledProvider()) {
        return;
    }
    if (!m_StateMachine.IsPlaying()) {
        return;
    }
    if (m_ValidationService->IsActive()) {
        return;
    }

    TASProject *project = m_PlaybackService->GetCurrentProject();
    const std::string outputPath = m_ValidationOutputPathBuilder
        ? m_ValidationOutputPathBuilder(project)
        : std::string{};
    if (outputPath.empty()) {
        return;
    }

    auto result = m_ValidationService->Start(outputPath, *m_PlaybackService);
    if (!result.IsOk()) {
        Log::Error("Validation recording: %s", result.GetError().message.c_str());
    }
}

void RuntimeEventRouter::HandlePlaybackCompleted(int playbackType) {
    if (!m_PlaybackService) {
        return;
    }

    const PlaybackType completedType = static_cast<PlaybackType>(playbackType);
    if (completedType != m_PlaybackService->GetPlaybackType()) {
        return;
    }
    if (!m_StateMachine.IsPlaying() && !m_StateMachine.IsPaused()) {
        return;
    }

    m_Requests.clearProjectOnStop = false;
    auto result = m_StateMachine.Transition(TASStateMachine::Event::Stop);
    if (!result.IsOk()) {
        Log::Error("State transition failed for playback completed: %s",
                   result.GetError().message.c_str());
    }
}

void RuntimeEventRouter::HandleTranslationCompleted() {
    if (!m_TranslationService || !m_StateMachine.IsTranslating()) {
        return;
    }

    m_Requests.clearProjectOnStop = false;
    auto result = m_StateMachine.Transition(TASStateMachine::Event::Stop);
    if (!result.IsOk()) {
        Log::Error("State transition failed for translation completed: %s",
                   result.GetError().message.c_str());
    }
}
