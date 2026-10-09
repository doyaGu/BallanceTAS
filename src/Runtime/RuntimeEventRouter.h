#pragma once

#include <functional>
#include <vector>

#include "EventBus.h"

class ContextLifecycleCoordinator;
struct OperationRequestStore;
class RuntimeSession;

class RuntimeEventRouter {
public:
    RuntimeEventRouter(
        EventBus &eventBus,
        RuntimeSession &runtimeSession,
        ContextLifecycleCoordinator &contextLifecycleCoordinator,
        OperationRequestStore &requests,
        std::function<bool()> levelLoadRunsTas);

    void Initialize();
    void Shutdown();

private:
    bool ArmedTasWaitsForAnotherLevel() const;
    void HandlePreLoadLevel();
    void HandleStartLevel();
    void HandlePlaybackCompleted(int playbackType);
    void HandleTranslationCompleted();

    EventBus &m_EventBus;
    RuntimeSession &m_RuntimeSession;
    ContextLifecycleCoordinator &m_ContextLifecycleCoordinator;
    OperationRequestStore &m_Requests;
    std::function<bool()> m_LevelLoadRunsTas;
    std::vector<ScopedSubscription> m_Subscriptions;
};
