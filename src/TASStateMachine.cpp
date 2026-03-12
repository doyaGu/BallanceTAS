#include "TASStateMachine.h"

#include <chrono>

#include "GameEvents.h"
#include "TASEngine.h"

TASStateMachine::TASStateMachine(TASEngine *engine)
    : m_Engine(engine), m_CurrentState(State::Idle), m_PreviousState(State::Idle) {
    InitializeTransitionTable();
}

void TASStateMachine::InitializeTransitionTable() {
    m_TransitionTable[{State::Idle, Event::StartRecording}] = State::PendingRecord;
    m_TransitionTable[{State::Idle, Event::StartScriptPlayback}] = State::PendingScriptPlayback;
    m_TransitionTable[{State::Idle, Event::StartRecordPlayback}] = State::PendingRecordPlayback;
    m_TransitionTable[{State::Idle, Event::StartTranslation}] = State::PendingTranslation;

    m_TransitionTable[{State::PendingRecord, Event::LevelStart}] = State::Recording;
    m_TransitionTable[{State::PendingScriptPlayback, Event::LevelStart}] = State::PlayingScript;
    m_TransitionTable[{State::PendingRecordPlayback, Event::LevelStart}] = State::PlayingRecord;
    m_TransitionTable[{State::PendingTranslation, Event::LevelStart}] = State::Translating;

    m_TransitionTable[{State::PendingRecord, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::PendingScriptPlayback, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::PendingRecordPlayback, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::PendingTranslation, Event::Stop}] = State::Idle;

    m_TransitionTable[{State::Recording, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::PlayingScript, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::PlayingRecord, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::Translating, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::Paused, Event::Stop}] = State::Idle;

    m_TransitionTable[{State::PlayingScript, Event::Pause}] = State::Paused;
    m_TransitionTable[{State::PlayingRecord, Event::Pause}] = State::Paused;
    m_TransitionTable[{State::Paused, Event::Resume}] = State::PlayingScript;

    m_TransitionTable[{State::PendingRecord, Event::LevelEnd}] = State::Idle;
    m_TransitionTable[{State::PendingScriptPlayback, Event::LevelEnd}] = State::Idle;
    m_TransitionTable[{State::PendingRecordPlayback, Event::LevelEnd}] = State::Idle;
    m_TransitionTable[{State::PendingTranslation, Event::LevelEnd}] = State::Idle;
    m_TransitionTable[{State::Recording, Event::LevelEnd}] = State::Idle;
    m_TransitionTable[{State::PlayingScript, Event::LevelEnd}] = State::Idle;
    m_TransitionTable[{State::PlayingRecord, Event::LevelEnd}] = State::Idle;
    m_TransitionTable[{State::Translating, Event::LevelEnd}] = State::Idle;
    m_TransitionTable[{State::Paused, Event::LevelEnd}] = State::Idle;

    for (auto state : {
             State::Idle,
             State::PendingRecord, State::PendingScriptPlayback,
             State::PendingRecordPlayback, State::PendingTranslation,
             State::Recording, State::PlayingScript, State::PlayingRecord,
             State::Translating, State::Paused
         }) {
        m_TransitionTable[{state, Event::Shutdown}] = State::ShuttingDown;
    }

    for (auto state : {
             State::PendingRecord, State::PendingScriptPlayback,
             State::PendingRecordPlayback, State::PendingTranslation,
             State::Recording, State::PlayingScript, State::PlayingRecord,
             State::Translating, State::Paused
         }) {
        m_TransitionTable[{state, Event::Error}] = State::Idle;
    }
}

Result<void> TASStateMachine::Transition(Event event) {
    const State requestedTargetState = FindTransitionTarget(m_CurrentState, event);

    if (requestedTargetState == m_CurrentState) {
        std::string errorMsg = std::string("Invalid state transition: ") +
            StateToString(m_CurrentState) + " -> " + EventToString(event);
        RecordTransition(m_CurrentState, event, m_CurrentState, false);
        return Result<void>::Error(errorMsg, "state_machine", ErrorSeverity::Warning);
    }

    if (auto handler = m_Handlers.find(m_CurrentState);
        handler != m_Handlers.end()) {
        if (!handler->second->CanTransitionTo(requestedTargetState)) {
            std::string errorMsg = std::string("Transition blocked by handler: ") +
                StateToString(m_CurrentState) + " -> " + StateToString(requestedTargetState);
            RecordTransition(m_CurrentState, event, requestedTargetState, false);
            return Result<void>::Error(errorMsg, "state_machine", ErrorSeverity::Warning);
        }
    }

    State oldState = m_CurrentState;
    return TransitionToState(requestedTargetState)
           .AndThen([this, event, oldState]() {
               RecordTransition(oldState, event, m_CurrentState, true);
               return Result<void>::Ok();
           })
           .OrElse([this, event, oldState, requestedTargetState](const ErrorInfo &error) {
               RecordTransition(oldState, event, requestedTargetState, false);
               return Result<void>::Error(error);
           });
}

Result<void> TASStateMachine::ForceSetState(State newState) {
    if (newState == m_CurrentState) {
        return Result<void>::Ok();
    }

    return TransitionToState(newState);
}

Result<void> TASStateMachine::TransitionToState(State newState) {
    State oldState = m_CurrentState;

    if (auto oldHandler = m_Handlers.find(oldState);
        oldHandler != m_Handlers.end()) {
        auto exitResult = oldHandler->second->OnExit(newState);
        if (!exitResult.IsOk()) {
            return exitResult;
        }
    }

    if (newState == State::Paused) {
        m_PreviousState = oldState;
    } else if (oldState == State::Paused && newState != State::Idle && newState != State::ShuttingDown) {
        newState = m_PreviousState;
    }

    m_CurrentState = newState;

    if (auto newHandler = m_Handlers.find(newState);
        newHandler != m_Handlers.end()) {
        auto enterResult = newHandler->second->OnEnter(oldState);
        if (!enterResult.IsOk()) {
            m_CurrentState = State::Idle;
            return Result<void>::Error(
                "Failed to enter state " + std::string(StateToString(newState)) +
                ": " + enterResult.GetError().message,
                "state_machine",
                ErrorSeverity::Error
            );
        }
    }

    if (m_Engine && m_Engine->GetEventBus()) {
        m_Engine->GetEventBus()->Publish(TASStateChangedEvent{
            static_cast<int>(oldState),
            static_cast<int>(m_CurrentState)
        });
    }

    return Result<void>::Ok();
}

TASStateMachine::State TASStateMachine::FindTransitionTarget(State currentState, Event event) const {
    auto it = m_TransitionTable.find({currentState, event});
    if (it != m_TransitionTable.end()) {
        return it->second;
    }
    return currentState;
}

bool TASStateMachine::IsTransitionValid(State from, State to) const {
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

    if (m_TransitionHistory.size() > MAX_HISTORY_SIZE) {
        m_TransitionHistory.erase(m_TransitionHistory.begin());
    }
}
