#include "LuaTypedEventBridge.h"

#include "Logger.h"
#include "Recorder.h"
#include "RuntimeSession.h"
#include "ScriptContextManager.h"

template <typename EventT>
static LuaGameEvent BuildLuaGameEvent(const EventT &, size_t tick) {
    return LuaGameEvent{
        EventTypeTraits<EventT>::type,
        EventTypeTraits<EventT>::name,
        tick
    };
}

template <>
static LuaGameEvent BuildLuaGameEvent(const PreCheckpointReachedEvent &event, size_t tick) {
    LuaGameEvent result{EventTypeTraits<PreCheckpointReachedEvent>::type,
                        EventTypeTraits<PreCheckpointReachedEvent>::name,
                        tick};
    result.sector = event.sector;
    return result;
}

template <>
static LuaGameEvent BuildLuaGameEvent(const PostCheckpointReachedEvent &event, size_t tick) {
    LuaGameEvent result{EventTypeTraits<PostCheckpointReachedEvent>::type,
                        EventTypeTraits<PostCheckpointReachedEvent>::name,
                        tick};
    result.sector = event.sector;
    return result;
}

template <>
static LuaGameEvent BuildLuaGameEvent(const ExtraPointEvent &event, size_t tick) {
    LuaGameEvent result{EventTypeTraits<ExtraPointEvent>::type,
                        EventTypeTraits<ExtraPointEvent>::name,
                        tick};
    result.points = event.points;
    return result;
}

template <>
static LuaGameEvent BuildLuaGameEvent(const PreSubLifeEvent &event, size_t tick) {
    LuaGameEvent result{EventTypeTraits<PreSubLifeEvent>::type,
                        EventTypeTraits<PreSubLifeEvent>::name,
                        tick};
    result.lifeCount = event.lifeCount;
    return result;
}

template <>
static LuaGameEvent BuildLuaGameEvent(const PostSubLifeEvent &event, size_t tick) {
    LuaGameEvent result{EventTypeTraits<PostSubLifeEvent>::type,
                        EventTypeTraits<PostSubLifeEvent>::name,
                        tick};
    result.lifeCount = event.lifeCount;
    return result;
}

template <>
static LuaGameEvent BuildLuaGameEvent(const PreLifeUpEvent &event, size_t tick) {
    LuaGameEvent result{EventTypeTraits<PreLifeUpEvent>::type,
                        EventTypeTraits<PreLifeUpEvent>::name,
                        tick};
    result.lifeCount = event.lifeCount;
    return result;
}

template <>
static LuaGameEvent BuildLuaGameEvent(const PostLifeUpEvent &event, size_t tick) {
    LuaGameEvent result{EventTypeTraits<PostLifeUpEvent>::type,
                        EventTypeTraits<PostLifeUpEvent>::name,
                        tick};
    result.lifeCount = event.lifeCount;
    return result;
}

LuaTypedEventBridge::LuaTypedEventBridge(
    EventBus &eventBus,
    ScriptContextManager &scriptContextManager,
    Recorder *recorder,
    RuntimeSession &runtimeSession,
    std::function<size_t()> currentTickProvider)
    : m_EventBus(eventBus),
      m_ScriptContextManager(scriptContextManager),
      m_Recorder(recorder),
      m_RuntimeSession(runtimeSession),
      m_CurrentTickProvider(std::move(currentTickProvider)) {
}

void LuaTypedEventBridge::Initialize() {
    m_Subscriptions.clear();

    // Simple no-data events
    SubscribeAndForward<PreStartMenuEvent>();
    SubscribeAndForward<PostStartMenuEvent>();
    SubscribeAndForward<PreLoadLevelEvent>();
    SubscribeAndForward<PostLoadLevelEvent>();
    SubscribeAndForward<StartLevelEvent>();
    SubscribeAndForward<PreResetLevelEvent>();
    SubscribeAndForward<PostResetLevelEvent>();
    SubscribeAndForward<PauseLevelEvent>();
    SubscribeAndForward<UnpauseLevelEvent>();
    SubscribeAndForward<PreExitLevelEvent>();
    SubscribeAndForward<PostExitLevelEvent>();
    SubscribeAndForward<PreNextLevelEvent>();
    SubscribeAndForward<PostNextLevelEvent>();
    SubscribeAndForward<PreEndLevelEvent>();
    SubscribeAndForward<PostEndLevelEvent>();
    SubscribeAndForward<LevelFinishEvent>();
    SubscribeAndForward<DeadEvent>();
    SubscribeAndForward<BallOffEvent>();
    SubscribeAndForward<GameOverEvent>();
    SubscribeAndForward<CounterActiveEvent>();
    SubscribeAndForward<CounterInactiveEvent>();
    SubscribeAndForward<BallNavActiveEvent>();
    SubscribeAndForward<BallNavInactiveEvent>();
    SubscribeAndForward<CamNavActiveEvent>();
    SubscribeAndForward<CamNavInactiveEvent>();

    // Data-carrying events
    SubscribeAndForward<PreCheckpointReachedEvent>();
    SubscribeAndForward<PostCheckpointReachedEvent>();
    SubscribeAndForward<ExtraPointEvent>();
    SubscribeAndForward<PreSubLifeEvent>();
    SubscribeAndForward<PostSubLifeEvent>();
    SubscribeAndForward<PreLifeUpEvent>();
    SubscribeAndForward<PostLifeUpEvent>();
}

void LuaTypedEventBridge::Shutdown() {
    m_Subscriptions.clear();
}

template <typename EventT>
void LuaTypedEventBridge::Forward(const EventT &event) {
    const LuaGameEvent luaEvent = BuildLuaGameEvent(event, m_CurrentTickProvider ? m_CurrentTickProvider() : 0);
    ForwardLuaEvent(luaEvent);
    RecordLegacyEvent(luaEvent);
}

void LuaTypedEventBridge::ForwardLuaEvent(const LuaGameEvent &event) {
    m_ScriptContextManager.DispatchGameEvent(event);
}

void LuaTypedEventBridge::RecordLegacyEvent(const LuaGameEvent &event) {
    if (!m_Recorder) {
        return;
    }
    if (!m_RuntimeSession.IsRecording() && !m_RuntimeSession.IsTranslating()) {
        return;
    }

    int payload = 0;
    if (event.sector.has_value()) {
        payload = *event.sector;
    } else if (event.points.has_value()) {
        payload = *event.points;
    } else if (event.lifeCount.has_value()) {
        payload = *event.lifeCount;
    }

    m_Recorder->OnGameEvent(event.tick, event.name, payload);
}
