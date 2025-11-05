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
    // 状态枚举
    enum class State {
        Idle,               // 空闲状态
        Recording,          // 录制中
        PlayingScript,      // 脚本回放中
        PlayingRecord,      // 记录回放中
        Translating,        // 翻译中（记录转脚本）
        Paused              // 暂停状态
    };

    // 事件枚举
    enum class Event {
        StartRecording,     // 开始录制
        StartScriptPlayback,// 开始脚本回放
        StartRecordPlayback,// 开始记录回放
        StartTranslation,   // 开始翻译
        Stop,               // 停止
        Pause,              // 暂停
        Resume,             // 继续
        LevelChange,        // 关卡切换
        Error               // 错误发生
    };

    // 状态处理器接口
    class IStateHandler {
    public:
        virtual ~IStateHandler() = default;

        // 进入状态时调用
        virtual Result<void> OnEnter() = 0;

        // 退出状态时调用
        virtual Result<void> OnExit() = 0;

        // 每帧调用
        virtual void OnTick() = 0;

        // 检查是否可以转换到新状态
        virtual bool CanTransitionTo(State newState) const = 0;

        // 获取状态名称（用于调试）
        virtual const char* GetStateName() const = 0;
    };

    explicit TASStateMachine(TASEngine* engine);
    ~TASStateMachine() = default;

    // 状态转换
    Result<void> Transition(Event event);

    // 强制设置状态（用于错误恢复）
    Result<void> ForceSetState(State newState);

    // 状态查询
    State GetCurrentState() const { return m_CurrentState; }
    const char* GetCurrentStateName() const;
    bool IsIdle() const { return m_CurrentState == State::Idle; }
    bool IsRecording() const { return m_CurrentState == State::Recording; }
    bool IsPlaying() const {
        return m_CurrentState == State::PlayingScript ||
               m_CurrentState == State::PlayingRecord;
    }
    bool IsTranslating() const { return m_CurrentState == State::Translating; }
    bool IsPaused() const { return m_CurrentState == State::Paused; }

    // 注册状态处理器
    void RegisterHandler(State state, std::unique_ptr<IStateHandler> handler);

    // 每帧调用
    void Tick();

    // 状态转换历史（用于调试）
    struct TransitionRecord {
        State fromState;
        Event event;
        State toState;
        uint64_t timestamp;
        bool succeeded;
    };

    const std::vector<TransitionRecord>& GetTransitionHistory() const {
        return m_TransitionHistory;
    }

    // 清空历史记录
    void ClearHistory() { m_TransitionHistory.clear(); }

    // 辅助函数
    static const char* StateToString(State state);
    static const char* EventToString(Event event);

private:
    // 执行状态转换
    Result<void> TransitionToState(State newState);

    // 查找转换目标
    State FindTransitionTarget(State currentState, Event event) const;

    // 验证转换合法性
    bool IsTransitionValid(State from, State to) const;

    TASEngine* m_Engine;
    State m_CurrentState;
    State m_PreviousState;  // 用于暂停/恢复

    // 状态处理器映射
    std::unordered_map<State, std::unique_ptr<IStateHandler>> m_Handlers;

    // 状态转换表
    struct StateEventPair {
        State state;
        Event event;

        bool operator==(const StateEventPair& other) const {
            return state == other.state && event == other.event;
        }
    };

    struct StateEventHash {
        size_t operator()(const StateEventPair& pair) const {
            return std::hash<int>()(static_cast<int>(pair.state)) ^
                   (std::hash<int>()(static_cast<int>(pair.event)) << 1);
        }
    };

    std::unordered_map<StateEventPair, State, StateEventHash> m_TransitionTable;

    // 转换历史
    std::vector<TransitionRecord> m_TransitionHistory;
    static constexpr size_t MAX_HISTORY_SIZE = 100;

    // 初始化转换表
    void InitializeTransitionTable();

    // 记录转换
    void RecordTransition(State fromState, Event event, State toState, bool succeeded);
};

// ============================================================================
// 辅助函数实现
// ============================================================================

inline const char* TASStateMachine::StateToString(State state) {
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

inline const char* TASStateMachine::EventToString(Event event) {
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

inline const char* TASStateMachine::GetCurrentStateName() const {
    return StateToString(m_CurrentState);
}
