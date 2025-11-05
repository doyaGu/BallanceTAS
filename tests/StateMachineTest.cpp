#include <gtest/gtest.h>
#include "TASStateMachine.h"
#include "Result.h"

// Mock TASEngine for testing
class MockTASEngine {
public:
    // 提供必要的接口供测试使用
};

// ============================================================================
// Mock State Handlers
// ============================================================================

class MockIdleHandler : public TASStateMachine::IStateHandler {
public:
    Result<void> OnEnter() override {
        enterCalled = true;
        return Result<void>::Ok();
    }

    Result<void> OnExit() override {
        exitCalled = true;
        return Result<void>::Ok();
    }

    void OnTick() override {
        tickCalled = true;
    }

    bool CanTransitionTo(TASStateMachine::State newState) const override {
        return true;  // 允许所有转换
    }

    const char* GetStateName() const override {
        return "Idle";
    }

    bool enterCalled = false;
    bool exitCalled = false;
    bool tickCalled = false;
};

class MockRecordingHandler : public TASStateMachine::IStateHandler {
public:
    Result<void> OnEnter() override {
        enterCalled = true;
        return Result<void>::Ok();
    }

    Result<void> OnExit() override {
        exitCalled = true;
        return Result<void>::Ok();
    }

    void OnTick() override {
        tickCalled = true;
    }

    bool CanTransitionTo(TASStateMachine::State newState) const override {
        // 录制中不能直接转换到播放状态
        return newState != TASStateMachine::State::PlayingScript &&
               newState != TASStateMachine::State::PlayingRecord;
    }

    const char* GetStateName() const override {
        return "Recording";
    }

    bool enterCalled = false;
    bool exitCalled = false;
    bool tickCalled = false;
};

// ============================================================================
// Basic State Machine Tests
// ============================================================================

TEST(StateMachineTest, InitialState) {
    MockTASEngine engine;
    TASStateMachine sm(reinterpret_cast<TASEngine*>(&engine));

    ASSERT_EQ(sm.GetCurrentState(), TASStateMachine::State::Idle);
    ASSERT_TRUE(sm.IsIdle());
    ASSERT_FALSE(sm.IsRecording());
    ASSERT_FALSE(sm.IsPlaying());
}

TEST(StateMachineTest, StateToString) {
    ASSERT_EQ(std::string(TASStateMachine::StateToString(TASStateMachine::State::Idle)), "Idle");
    ASSERT_EQ(std::string(TASStateMachine::StateToString(TASStateMachine::State::Recording)), "Recording");
    ASSERT_EQ(std::string(TASStateMachine::StateToString(TASStateMachine::State::PlayingScript)), "PlayingScript");
}

TEST(StateMachineTest, EventToString) {
    ASSERT_EQ(std::string(TASStateMachine::EventToString(TASStateMachine::Event::StartRecording)), "StartRecording");
    ASSERT_EQ(std::string(TASStateMachine::EventToString(TASStateMachine::Event::Stop)), "Stop");
}

// ============================================================================
// State Transition Tests
// ============================================================================

TEST(TransitionTest, BasicTransition) {
    MockTASEngine engine;
    TASStateMachine sm(reinterpret_cast<TASEngine*>(&engine));

    // Register handlers
    auto idleHandler = std::make_unique<MockIdleHandler>();
    auto recordHandler = std::make_unique<MockRecordingHandler>();
    auto* idlePtr = idleHandler.get();
    auto* recordPtr = recordHandler.get();

    sm.RegisterHandler(TASStateMachine::State::Idle, std::move(idleHandler));
    sm.RegisterHandler(TASStateMachine::State::Recording, std::move(recordHandler));

    // Transition from Idle to Recording
    auto result = sm.Transition(TASStateMachine::Event::StartRecording);
    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(sm.GetCurrentState(), TASStateMachine::State::Recording);
    ASSERT_TRUE(sm.IsRecording());

    // Verify handlers were called
    ASSERT_TRUE(idlePtr->exitCalled);
    ASSERT_TRUE(recordPtr->enterCalled);
}

TEST(TransitionTest, InvalidTransition) {
    MockTASEngine engine;
    TASStateMachine sm(reinterpret_cast<TASEngine*>(&engine));

    // Try invalid transition from Idle (e.g., direct Resume)
    auto result = sm.Transition(TASStateMachine::Event::Resume);
    ASSERT_TRUE(result.IsError());
    ASSERT_EQ(sm.GetCurrentState(), TASStateMachine::State::Idle);
}

TEST(TransitionTest, StopTransition) {
    MockTASEngine engine;
    TASStateMachine sm(reinterpret_cast<TASEngine*>(&engine));

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::Recording, std::make_unique<MockRecordingHandler>());

    // Idle -> Recording
    sm.Transition(TASStateMachine::Event::StartRecording);
    ASSERT_TRUE(sm.IsRecording());

    // Recording -> Idle (Stop)
    auto result = sm.Transition(TASStateMachine::Event::Stop);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(sm.IsIdle());
}

TEST(TransitionTest, MultipleTransitions) {
    MockTASEngine engine;
    TASStateMachine sm(reinterpret_cast<TASEngine*>(&engine));

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::Recording, std::make_unique<MockRecordingHandler>());

    // Idle -> Recording
    sm.Transition(TASStateMachine::Event::StartRecording);
    ASSERT_TRUE(sm.IsRecording());

    // Recording -> Idle
    sm.Transition(TASStateMachine::Event::Stop);
    ASSERT_TRUE(sm.IsIdle());

    // Idle -> Recording again
    sm.Transition(TASStateMachine::Event::StartRecording);
    ASSERT_TRUE(sm.IsRecording());
}

// ============================================================================
// Handler Tests
// ============================================================================

TEST(HandlerTest, Blocking) {
    MockTASEngine engine;
    TASStateMachine sm(reinterpret_cast<TASEngine*>(&engine));

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::Recording, std::make_unique<MockRecordingHandler>());

    // Idle -> Recording
    sm.Transition(TASStateMachine::Event::StartRecording);
    ASSERT_TRUE(sm.IsRecording());

    // Try to transition from Recording to PlayingScript (should be blocked by Handler)
    auto result = sm.Transition(TASStateMachine::Event::StartScriptPlayback);
    ASSERT_TRUE(result.IsError());  // Blocked
    ASSERT_TRUE(sm.IsRecording());  // State unchanged
}

TEST(HandlerTest, TickCalling) {
    MockTASEngine engine;
    TASStateMachine sm(reinterpret_cast<TASEngine*>(&engine));

    auto handler = std::make_unique<MockRecordingHandler>();
    auto* handlerPtr = handler.get();
    sm.RegisterHandler(TASStateMachine::State::Recording, std::move(handler));
    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());

    // Transition to Recording state
    sm.Transition(TASStateMachine::Event::StartRecording);

    // Call Tick
    sm.Tick();

    ASSERT_TRUE(handlerPtr->tickCalled);
}

// ============================================================================
// Transition History Tests
// ============================================================================

TEST(HistoryTest, TransitionHistory) {
    MockTASEngine engine;
    TASStateMachine sm(reinterpret_cast<TASEngine*>(&engine));

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::Recording, std::make_unique<MockRecordingHandler>());

    // Execute several transitions
    sm.Transition(TASStateMachine::Event::StartRecording);
    sm.Transition(TASStateMachine::Event::Stop);

    const auto& history = sm.GetTransitionHistory();
    ASSERT_EQ(history.size(), 2);

    // Check first transition
    ASSERT_EQ(history[0].fromState, TASStateMachine::State::Idle);
    ASSERT_EQ(history[0].event, TASStateMachine::Event::StartRecording);
    ASSERT_EQ(history[0].toState, TASStateMachine::State::Recording);
    ASSERT_TRUE(history[0].succeeded);

    // Check second transition
    ASSERT_EQ(history[1].fromState, TASStateMachine::State::Recording);
    ASSERT_EQ(history[1].event, TASStateMachine::Event::Stop);
    ASSERT_EQ(history[1].toState, TASStateMachine::State::Idle);
    ASSERT_TRUE(history[1].succeeded);
}

TEST(HistoryTest, ClearHistory) {
    MockTASEngine engine;
    TASStateMachine sm(reinterpret_cast<TASEngine*>(&engine));

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::Recording, std::make_unique<MockRecordingHandler>());

    sm.Transition(TASStateMachine::Event::StartRecording);
    ASSERT_EQ(sm.GetTransitionHistory().size(), 1);

    sm.ClearHistory();
    ASSERT_EQ(sm.GetTransitionHistory().size(), 0);
}

// ============================================================================
// Force Set State Tests
// ============================================================================

TEST(ForceSetStateTest, ForceSetState) {
    MockTASEngine engine;
    TASStateMachine sm(reinterpret_cast<TASEngine*>(&engine));

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::Recording, std::make_unique<MockRecordingHandler>());

    // Force set state (skip validation)
    auto result = sm.ForceSetState(TASStateMachine::State::Recording);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(sm.IsRecording());
}
