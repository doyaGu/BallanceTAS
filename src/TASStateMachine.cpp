#include "TASStateMachine.h"
#include <chrono>

TASStateMachine::TASStateMachine(TASEngine *engine)
    : m_Engine(engine), m_CurrentState(State::Idle), m_PreviousState(State::Idle) {
    InitializeTransitionTable();
}

void TASStateMachine::InitializeTransitionTable() {
    // 从Idle可以转换到任何活动状态
    m_TransitionTable[{State::Idle, Event::StartRecording}] = State::Recording;
    m_TransitionTable[{State::Idle, Event::StartScriptPlayback}] = State::PlayingScript;
    m_TransitionTable[{State::Idle, Event::StartRecordPlayback}] = State::PlayingRecord;
    m_TransitionTable[{State::Idle, Event::StartTranslation}] = State::Translating;

    // 从任何活动状态可以停止到Idle
    m_TransitionTable[{State::Recording, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::PlayingScript, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::PlayingRecord, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::Translating, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::Paused, Event::Stop}] = State::Idle;

    // 暂停和恢复
    m_TransitionTable[{State::PlayingScript, Event::Pause}] = State::Paused;
    m_TransitionTable[{State::PlayingRecord, Event::Pause}] = State::Paused;
    m_TransitionTable[{State::Paused, Event::Resume}] = State::PlayingScript; // 默认恢复到脚本播放

    // 关卡切换会停止当前操作
    m_TransitionTable[{State::Recording, Event::LevelChange}] = State::Idle;
    m_TransitionTable[{State::PlayingScript, Event::LevelChange}] = State::Idle;
    m_TransitionTable[{State::PlayingRecord, Event::LevelChange}] = State::Idle;
    m_TransitionTable[{State::Translating, Event::LevelChange}] = State::Idle;

    // 错误处理 - 任何状态遇到错误都返回Idle
    for (auto state : {
             State::Recording, State::PlayingScript, State::PlayingRecord,
             State::Translating, State::Paused
         }) {
        m_TransitionTable[{state, Event::Error}] = State::Idle;
    }
}

Result<void> TASStateMachine::Transition(Event event) {
    State targetState = FindTransitionTarget(m_CurrentState, event);

    // 检查转换是否在转换表中定义
    if (targetState == m_CurrentState) {
        std::string errorMsg = std::string("Invalid state transition: ") +
            StateToString(m_CurrentState) + " -> " +
            EventToString(event);
        RecordTransition(m_CurrentState, event, m_CurrentState, false);
        return Result<void>::Error(errorMsg, "state_machine", ErrorSeverity::Warning);
    }

    // 检查当前状态的处理器是否允许转换
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

    // 执行转换
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

    // 强制转换，跳过验证
    return TransitionToState(newState);
}

Result<void> TASStateMachine::TransitionToState(State newState) {
    State oldState = m_CurrentState;

    // 1. 退出旧状态
    if (auto oldHandler = m_Handlers.find(oldState);
        oldHandler != m_Handlers.end()) {
        auto exitResult = oldHandler->second->OnExit();
        if (!exitResult.IsOk()) {
            // 退出失败，记录错误但继续
            // Log::Warn("Failed to exit state %s: %s",
            //          StateToString(oldState),
            //          exitResult.GetError().Format().c_str());
        }
    }

    // 2. 更新状态
    if (newState == State::Paused) {
        m_PreviousState = oldState; // 保存暂停前的状态
    } else if (oldState == State::Paused && newState != State::Idle) {
        // 从暂停恢复，使用之前保存的状态
        newState = m_PreviousState;
    }

    m_CurrentState = newState;

    // Log::Info("State transition: %s -> %s",
    //          StateToString(oldState),
    //          StateToString(newState));

    // 3. 进入新状态
    if (auto newHandler = m_Handlers.find(newState);
        newHandler != m_Handlers.end()) {
        auto enterResult = newHandler->second->OnEnter();
        if (!enterResult.IsOk()) {
            // 进入失败，尝试回滚到Idle
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
    return currentState; // 无效转换，返回当前状态
}

bool TASStateMachine::IsTransitionValid(State from, State to) const {
    // 检查是否存在能导致此转换的事件
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
    // 调用当前状态的Tick处理器
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

    // 限制历史记录大小
    if (m_TransitionHistory.size() > MAX_HISTORY_SIZE) {
        m_TransitionHistory.erase(m_TransitionHistory.begin());
    }
}
