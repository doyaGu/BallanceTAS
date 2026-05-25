#pragma once

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
        OperationRequestStore &requests);

    void Initialize();
    void Shutdown();

private:
    void HandlePreLoadLevel();
    void HandleStartLevel();
    void HandlePlaybackCompleted(int playbackType);
    void HandleTranslationCompleted();

    EventBus &m_EventBus;
    RuntimeSession &m_RuntimeSession;
    ContextLifecycleCoordinator &m_ContextLifecycleCoordinator;
    OperationRequestStore &m_Requests;
    std::vector<ScopedSubscription> m_Subscriptions;
};
