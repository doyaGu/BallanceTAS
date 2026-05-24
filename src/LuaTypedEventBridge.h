#pragma once

#include <functional>
#include <vector>

#include "EventBus.h"
#include "GameEvents.h"

class Recorder;
class RuntimeSession;
class ScriptContextManager;

class LuaTypedEventBridge {
public:
    LuaTypedEventBridge(
        EventBus &eventBus,
        ScriptContextManager &scriptContextManager,
        Recorder *recorder,
        RuntimeSession &runtimeSession,
        std::function<size_t()> currentTickProvider);

    void Initialize();
    void Shutdown();

private:
    template <typename EventT>
    void SubscribeAndForward() {
        m_Subscriptions.push_back(m_EventBus.Subscribe<EventT>(
            [this](const EventT &event) { Forward(event); }));
    }

    template <typename EventT>
    void Forward(const EventT &event);

    void ForwardLuaEvent(const LuaGameEvent &event);
    void RecordLegacyEvent(const LuaGameEvent &event);

    EventBus &m_EventBus;
    ScriptContextManager &m_ScriptContextManager;
    Recorder *m_Recorder = nullptr;
    RuntimeSession &m_RuntimeSession;
    std::function<size_t()> m_CurrentTickProvider;
    std::vector<ScopedSubscription> m_Subscriptions;
};
