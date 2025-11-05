#include "TASStateMachine.h"
#include <chrono>

TASStateMachine::TASStateMachine(TASEngine *engine)
    : m_Engine(engine), m_CurrentState(State::Idle), m_PreviousState(State::Idle) {
    InitializeTransitionTable();
}

void TASStateMachine::InitializeTransitionTable() {
    // From Idle can transition to any active state
    m_TransitionTable[{State::Idle, Event::StartRecording}] = State::Recording;
    m_TransitionTable[{State::Idle, Event::StartScriptPlayback}] = State::PlayingScript;
    m_TransitionTable[{State::Idle, Event::StartRecordPlayback}] = State::PlayingRecord;
    m_TransitionTable[{State::Idle, Event::StartTranslation}] = State::Translating;

    // From any active state can stop to Idle
    m_TransitionTable[{State::Recording, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::PlayingScript, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::PlayingRecord, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::Translating, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::Paused, Event::Stop}] = State::Idle;

    // Pause and resume
    m_TransitionTable[{State::PlayingScript, Event::Pause}] = State::Paused;
    m_TransitionTable[{State::PlayingRecord, Event::Pause}] = State::Paused;
    m_TransitionTable[{State::Paused, Event::Resume}] = State::PlayingScript; // Default resume to script playback

    // Level change will stop current operation
    m_TransitionTable[{State::Recording, Event::LevelChange}] = State::Idle;
    m_TransitionTable[{State::PlayingScript, Event::LevelChange}] = State::Idle;
    m_TransitionTable[{State::PlayingRecord, Event::LevelChange}] = State::Idle;
    m_TransitionTable[{State::Translating, Event::LevelChange}] = State::Idle;

    // Error handling - any state encountering error returns to Idle
    for (auto state : {
             State::Recording, State::PlayingScript, State::PlayingRecord,
             State::Translating, State::Paused
         }) {
        m_TransitionTable[{state, Event::Error}] = State::Idle;
    }
}

Result<void> TASStateMachine::Transition(Event event) {
    State targetState = FindTransitionTarget(m_CurrentState, event);

    // Check if transition is defined in transition table
    if (targetState == m_CurrentState) {
        std::string errorMsg = std::string("Invalid state transition: ") +
            StateToString(m_CurrentState) + " -> " +
            EventToString(event);
        RecordTransition(m_CurrentState, event, m_CurrentState, false);
        return Result<void>::Error(errorMsg, "state_machine", ErrorSeverity::Warning);
    }

    // Check if current state handler allows transition
    if (auto handler = m_Handlers.find(m_CurrentState);
        handler != m_Handlers.end()) {
        if (!handler->second->CanTransitionTo(targetState)) {
            std::string errorMsg = std::string("Transition blocked by handler: ") +
                StateToString(m_CurrentState) + " -> " +
                StateToString(targetState);
            RecordTransition(m_CurrentState, event, targetState, false);
            return Result<void>::Error(errorMsg, "state_machine", ErrorSeverity::Warning);
        }
    }

    // Execute transition
    State oldState = m_CurrentState; // Capture state before transition
    return TransitionToState(targetState)
           .AndThen([this, event, oldState, targetState]() {
               RecordTransition(oldState, event, targetState, true);
               return Result<void>::Ok();
           })
           .OrElse([this, event, oldState, targetState](const ErrorInfo &error) {
               RecordTransition(oldState, event, targetState, false);
               return Result<void>::Error(error);
           });
}

Result<void> TASStateMachine::ForceSetState(State newState) {
    if (newState == m_CurrentState) {
        return Result<void>::Ok();
    }

    // Force transition, skip validation
    return TransitionToState(newState);
}

Result<void> TASStateMachine::TransitionToState(State newState) {
    State oldState = m_CurrentState;

    // 1. Exit old state
    if (auto oldHandler = m_Handlers.find(oldState);
        oldHandler != m_Handlers.end()) {
        auto exitResult = oldHandler->second->OnExit();
        if (!exitResult.IsOk()) {
            // Exit failed, log error but continue
            // Log::Warn("Failed to exit state %s: %s",
            //          StateToString(oldState),
            //          exitResult.GetError().Format().c_str());
        }
    }

    // 2. Update state
    if (newState == State::Paused) {
        m_PreviousState = oldState; // Save state before pause
    } else if (oldState == State::Paused && newState != State::Idle) {
        // Resume from pause, use previously saved state
        newState = m_PreviousState;
    }

    m_CurrentState = newState;

    // Log::Info("State transition: %s -> %s",
    //          StateToString(oldState),
    //          StateToString(newState));

    // 3. Enter new state
    if (auto newHandler = m_Handlers.find(newState);
        newHandler != m_Handlers.end()) {
        auto enterResult = newHandler->second->OnEnter();
        if (!enterResult.IsOk()) {
            // Entry failed, try to rollback to Idle
            m_CurrentState = State::Idle;
            return Result<void>::Error(
                "Failed to enter state " + std::string(StateToString(newState)) +
                ": " + enterResult.GetError().message,
                "state_machine",
                ErrorSeverity::Error
            );
        }
    }

    return Result<void>::Ok();
}

TASStateMachine::State TASStateMachine::FindTransitionTarget(State currentState, Event event) const {
    auto it = m_TransitionTable.find({currentState, event});
    if (it != m_TransitionTable.end()) {
        return it->second;
    }
    return currentState; // Invalid transition, return current state
}

bool TASStateMachine::IsTransitionValid(State from, State to) const {
    // Check if there exists an event that can cause this transition
    for (const auto &[pair, targetState] : m_TransitionTable) {
        if (pair.state == from && targetState == to) {
            return true;
        }
    }
    return false;
}

void TASStateMachine::RegisterHandler(State state, std::unique_ptr<IStateHandler> handler) {
    m_Handlers[state] = std::move(handler);
}

void TASStateMachine::Tick() {
    // Call Tick handler of current state
    if (auto handler = m_Handlers.find(m_CurrentState);
        handler != m_Handlers.end()) {
        handler->second->OnTick();
    }
}

void TASStateMachine::RecordTransition(State fromState, Event event, State toState, bool succeeded) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    m_TransitionHistory.push_back({
        fromState,
        event,
        toState,
        static_cast<uint64_t>(timestamp),
        succeeded
    });

    // Limit history size
    if (m_TransitionHistory.size() > MAX_HISTORY_SIZE) {
        m_TransitionHistory.erase(m_TransitionHistory.begin());
    }
}
