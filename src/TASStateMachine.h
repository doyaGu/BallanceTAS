#pragma once

#include "Result.h"
#include <memory>
#include <unordered_map>
#include <functional>
#include <string>

// Forward declarations
class TASEngine;

// ============================================================================
// TAS State Machine
// ============================================================================
class TASStateMachine {
public:
    // State enumeration
    enum class State {
        Idle,          // Idle state
        Recording,     // Recording
        PlayingScript, // Script playback
        PlayingRecord, // Record playback
        Translating,   // Translating (record to script)
        Paused         // Paused state
    };

    // Event enumeration
    enum class Event {
        StartRecording,      // Start recording
        StartScriptPlayback, // Start script playback
        StartRecordPlayback, // Start record playback
        StartTranslation,    // Start translation
        Stop,                // Stop
        Pause,               // Pause
        Resume,              // Resume
        LevelChange,         // Level change
        Error                // Error occurred
    };

    // State handler interface
    class IStateHandler {
    public:
        virtual ~IStateHandler() = default;

        // Called when entering state
        virtual Result<void> OnEnter() = 0;

        // Called when exiting state
        virtual Result<void> OnExit() = 0;

        // Called every frame
        virtual void OnTick() = 0;

        // Check if can transition to new state
        virtual bool CanTransitionTo(State newState) const = 0;

        // Get state name (for debugging)
        virtual const char *GetStateName() const = 0;
    };

    explicit TASStateMachine(TASEngine *engine);
    ~TASStateMachine() = default;

    // State transition
    Result<void> Transition(Event event);

    // Force set state (for error recovery)
    Result<void> ForceSetState(State newState);

    // Status query
    State GetCurrentState() const { return m_CurrentState; }
    const char *GetCurrentStateName() const;
    bool IsIdle() const { return m_CurrentState == State::Idle; }
    bool IsRecording() const { return m_CurrentState == State::Recording; }

    bool IsPlaying() const {
        return m_CurrentState == State::PlayingScript ||
            m_CurrentState == State::PlayingRecord;
    }

    bool IsTranslating() const { return m_CurrentState == State::Translating; }
    bool IsPaused() const { return m_CurrentState == State::Paused; }

    // Register state handler
    void RegisterHandler(State state, std::unique_ptr<IStateHandler> handler);

    // Called every frame
    void Tick();

    // State transition history (for debugging)
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

    // Clear history
    void ClearHistory() { m_TransitionHistory.clear(); }

    // Helper functions
    static const char *StateToString(State state);
    static const char *EventToString(Event event);

private:
    // Execute state transition
    Result<void> TransitionToState(State newState);

    // Find transition target
    State FindTransitionTarget(State currentState, Event event) const;

    // Validate transition legitimacy
    bool IsTransitionValid(State from, State to) const;

    TASEngine *m_Engine;
    State m_CurrentState;
    State m_PreviousState; // Used for pause/resume

    // State handler mapping
    std::unordered_map<State, std::unique_ptr<IStateHandler>> m_Handlers;

    // State transition table
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

    // Transition history
    std::vector<TransitionRecord> m_TransitionHistory;
    static constexpr size_t MAX_HISTORY_SIZE = 100;

    // Initialize transition table
    void InitializeTransitionTable();

    // Record transition
    void RecordTransition(State fromState, Event event, State toState, bool succeeded);
};

// ============================================================================
// Helper function implementation
// ============================================================================

inline const char *TASStateMachine::StateToString(State state) {
    switch (state) {
    case State::Idle: return "Idle";
    case State::Recording: return "Recording";
    case State::PlayingScript: return "PlayingScript";
    case State::PlayingRecord: return "PlayingRecord";
    case State::Translating: return "Translating";
    case State::Paused: return "Paused";
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
    case Event::LevelChange: return "LevelChange";
    case Event::Error: return "Error";
    default: return "Unknown";
    }
}

inline const char *TASStateMachine::GetCurrentStateName() const {
    return StateToString(m_CurrentState);
}
