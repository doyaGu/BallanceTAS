#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include "Result.h"

class TASEngine;

class TASStateMachine {
public:
    enum class State {
        Idle,
        PendingRecord,
        Recording,
        PendingScriptPlayback,
        PendingRecordPlayback,
        PlayingScript,
        PlayingRecord,
        PendingTranslation,
        Translating,
        Paused,
        ShuttingDown
    };

    enum class Event {
        StartRecording,
        StartScriptPlayback,
        StartRecordPlayback,
        StartTranslation,
        Stop,
        Pause,
        Resume,
        LevelStart,
        LevelEnd,
        Shutdown,
        Error
    };

    class IStateHandler {
    public:
        virtual ~IStateHandler() = default;

        virtual Result<void> OnEnter(State previousState) { return OnEnter(); }
        virtual Result<void> OnEnter() { return Result<void>::Ok(); }

        virtual Result<void> OnExit(State nextState) { return OnExit(); }
        virtual Result<void> OnExit() { return Result<void>::Ok(); }

        virtual void OnTick() = 0;
        virtual bool CanTransitionTo(State newState) const = 0;
        virtual const char *GetStateName() const = 0;
    };

    explicit TASStateMachine(TASEngine *engine);
    ~TASStateMachine() = default;

    Result<void> Transition(Event event);
    Result<void> ForceSetState(State newState);

    State GetCurrentState() const { return m_CurrentState; }
    State GetPreviousState() const { return m_PreviousState; }
    const char *GetCurrentStateName() const;

    bool IsIdle() const { return m_CurrentState == State::Idle; }
    bool IsRecording() const { return m_CurrentState == State::Recording; }
    bool IsPlaying() const {
        return m_CurrentState == State::PlayingScript ||
               m_CurrentState == State::PlayingRecord;
    }
    bool IsTranslating() const { return m_CurrentState == State::Translating; }
    bool IsPaused() const { return m_CurrentState == State::Paused; }
    bool IsShuttingDown() const { return m_CurrentState == State::ShuttingDown; }
    bool IsPending() const {
        return m_CurrentState == State::PendingRecord ||
               m_CurrentState == State::PendingScriptPlayback ||
               m_CurrentState == State::PendingRecordPlayback ||
               m_CurrentState == State::PendingTranslation;
    }
    bool IsPendingRecord() const { return m_CurrentState == State::PendingRecord; }
    bool IsPendingPlay() const {
        return m_CurrentState == State::PendingScriptPlayback ||
               m_CurrentState == State::PendingRecordPlayback;
    }
    bool IsPendingTranslate() const { return m_CurrentState == State::PendingTranslation; }

    void RegisterHandler(State state, std::unique_ptr<IStateHandler> handler);
    void Tick();

    struct TransitionRecord {
        State fromState;
        Event event;
        State toState;
        uint64_t timestamp;
        bool succeeded;
    };

    const std::vector<TransitionRecord> &GetTransitionHistory() const {
        return m_TransitionHistory;
    }

    void ClearHistory() { m_TransitionHistory.clear(); }

    static const char *StateToString(State state);
    static const char *EventToString(Event event);

private:
    Result<void> TransitionToState(State newState);
    State FindTransitionTarget(State currentState, Event event) const;
    bool IsTransitionValid(State from, State to) const;

    TASEngine *m_Engine;
    State m_CurrentState;
    State m_PreviousState;

    std::unordered_map<State, std::unique_ptr<IStateHandler>> m_Handlers;

    struct StateEventPair {
        State state;
        Event event;

        bool operator==(const StateEventPair &other) const {
            return state == other.state && event == other.event;
        }
    };

    struct StateEventHash {
        size_t operator()(const StateEventPair &pair) const {
            return std::hash<int>()(static_cast<int>(pair.state)) ^
                   (std::hash<int>()(static_cast<int>(pair.event)) << 1);
        }
    };

    std::unordered_map<StateEventPair, State, StateEventHash> m_TransitionTable;
    std::vector<TransitionRecord> m_TransitionHistory;
    static constexpr size_t MAX_HISTORY_SIZE = 100;

    void InitializeTransitionTable();
    void RecordTransition(State fromState, Event event, State toState, bool succeeded);
};

inline const char *TASStateMachine::StateToString(State state) {
    switch (state) {
    case State::Idle: return "Idle";
    case State::PendingRecord: return "PendingRecord";
    case State::Recording: return "Recording";
    case State::PendingScriptPlayback: return "PendingScriptPlayback";
    case State::PendingRecordPlayback: return "PendingRecordPlayback";
    case State::PlayingScript: return "PlayingScript";
    case State::PlayingRecord: return "PlayingRecord";
    case State::PendingTranslation: return "PendingTranslation";
    case State::Translating: return "Translating";
    case State::Paused: return "Paused";
    case State::ShuttingDown: return "ShuttingDown";
    default: return "Unknown";
    }
}

inline const char *TASStateMachine::EventToString(Event event) {
    switch (event) {
    case Event::StartRecording: return "StartRecording";
    case Event::StartScriptPlayback: return "StartScriptPlayback";
    case Event::StartRecordPlayback: return "StartRecordPlayback";
    case Event::StartTranslation: return "StartTranslation";
    case Event::Stop: return "Stop";
    case Event::Pause: return "Pause";
    case Event::Resume: return "Resume";
    case Event::LevelStart: return "LevelStart";
    case Event::LevelEnd: return "LevelEnd";
    case Event::Shutdown: return "Shutdown";
    case Event::Error: return "Error";
    default: return "Unknown";
    }
}

inline const char *TASStateMachine::GetCurrentStateName() const {
    return StateToString(m_CurrentState);
}
