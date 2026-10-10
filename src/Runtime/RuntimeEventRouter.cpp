#include "RuntimeEventRouter.h"

#include "ContextLifecycleCoordinator.h"
#include "GameEvents.h"
#include "Logger.h"
#include "OperationRequestStore.h"
#include "PlaybackTypes.h"
#include "RuntimeSession.h"

RuntimeEventRouter::RuntimeEventRouter(
    EventBus &eventBus,
    RuntimeSession &runtimeSession,
    ContextLifecycleCoordinator &contextLifecycleCoordinator,
    OperationRequestStore &requests,
    std::function<bool()> levelLoadRunsTas)
    : m_EventBus(eventBus),
      m_RuntimeSession(runtimeSession),
      m_ContextLifecycleCoordinator(contextLifecycleCoordinator),
      m_Requests(requests),
      m_LevelLoadRunsTas(std::move(levelLoadRunsTas)) {
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
    if (!m_RuntimeSession.IsPending()) {
        return;
    }
    if (ArmedTasWaitsForAnotherLevel()) {
        return;
    }

    auto result = m_RuntimeSession.OnLevelLoadStart();
    if (!result.IsOk() && result.GetError().severity != ErrorSeverity::Warning) {
        Log::Error("State transition failed for level load start: %s",
                   result.GetError().message.c_str());
    }
}

// A re-armed project keeps waiting for its own level. "Next level" starts a level without a
// PreLoadLevel, so both load paths ask.
bool RuntimeEventRouter::ArmedTasWaitsForAnotherLevel() const {
    if (!m_RuntimeSession.IsPending() || !m_LevelLoadRunsTas || m_LevelLoadRunsTas()) {
        return false;
    }
    if (m_Requests.restartLevel > 0) {
        Log::Info("Armed TAS waits for Level %d to load; this level runs without it.", m_Requests.restartLevel);
    } else {
        Log::Info("Armed TAS waits for the next level load; this level runs without it.");
    }
    return true;
}

void RuntimeEventRouter::HandleStartLevel() {
    if (ArmedTasWaitsForAnotherLevel()) {
        return;
    }
    auto result = m_RuntimeSession.OnLevelStart();
    if (!result.IsOk() && result.GetError().severity != ErrorSeverity::Warning) {
        Log::Error("State transition failed for level start: %s",
                   result.GetError().message.c_str());
    }
}

void RuntimeEventRouter::HandlePlaybackCompleted(int playbackType) {
    const PlaybackType completedType = static_cast<PlaybackType>(playbackType);
    m_Requests.clearProjectOnStop = false;
    auto result = m_RuntimeSession.OnPlaybackCompleted(completedType);
    if (!result.IsOk()) {
        Log::Error("State transition failed for playback completed: %s",
                   result.GetError().message.c_str());
    }
}

void RuntimeEventRouter::HandleTranslationCompleted() {
    m_Requests.clearProjectOnStop = false;
    auto result = m_RuntimeSession.OnTranslationCompleted();
    if (!result.IsOk()) {
        Log::Error("State transition failed for translation completed: %s",
                   result.GetError().message.c_str());
    }
}
