#pragma once

#include <functional>
#include <string>
#include <vector>

#include "EventBus.h"

class ContextLifecycleCoordinator;
struct OperationRequestStore;
class PlaybackService;
class TASProject;
class TASStateMachine;
class TranslationService;
class ValidationService;

class RuntimeEventRouter {
public:
    RuntimeEventRouter(
        EventBus &eventBus,
        TASStateMachine &stateMachine,
        ContextLifecycleCoordinator &contextLifecycleCoordinator,
        PlaybackService *playbackService,
        TranslationService *translationService,
        ValidationService *validationService,
        OperationRequestStore &requests,
        std::function<bool()> validationEnabledProvider,
        std::function<std::string(TASProject *)> validationOutputPathBuilder);

    void Initialize();
    void Shutdown();

private:
    void HandlePreLoadLevel();
    void HandleStartLevel();
    void HandlePlaybackCompleted(int playbackType);
    void HandleTranslationCompleted();

    EventBus &m_EventBus;
    TASStateMachine &m_StateMachine;
    ContextLifecycleCoordinator &m_ContextLifecycleCoordinator;
    PlaybackService *m_PlaybackService = nullptr;
    TranslationService *m_TranslationService = nullptr;
    ValidationService *m_ValidationService = nullptr;
    OperationRequestStore &m_Requests;
    std::function<bool()> m_ValidationEnabledProvider;
    std::function<std::string(TASProject *)> m_ValidationOutputPathBuilder;
    std::vector<ScopedSubscription> m_Subscriptions;
};
