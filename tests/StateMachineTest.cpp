#include <gtest/gtest.h>

#include "Result.h"
#include "TASStateMachine.h"

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

    bool CanTransitionTo(TASStateMachine::State) const override {
        return true;
    }

    const char *GetStateName() const override {
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
        return newState != TASStateMachine::State::PlayingScript &&
               newState != TASStateMachine::State::PlayingRecord;
    }

    const char *GetStateName() const override {
        return "Recording";
    }

    bool enterCalled = false;
    bool exitCalled = false;
    bool tickCalled = false;
};

class MockPendingRecordHandler : public TASStateMachine::IStateHandler {
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

    bool CanTransitionTo(TASStateMachine::State) const override {
        return true;
    }

    const char *GetStateName() const override {
        return "PendingRecord";
    }

    bool enterCalled = false;
    bool exitCalled = false;
    bool tickCalled = false;
};

TEST(StateMachineTest, InitialState) {
    TASStateMachine sm(nullptr);

    ASSERT_EQ(sm.GetCurrentState(), TASStateMachine::State::Idle);
    ASSERT_TRUE(sm.IsIdle());
    ASSERT_FALSE(sm.IsRecording());
    ASSERT_FALSE(sm.IsPlaying());
    ASSERT_FALSE(sm.IsPending());
}

TEST(StateMachineTest, StateToString) {
    ASSERT_EQ(std::string(TASStateMachine::StateToString(TASStateMachine::State::Idle)), "Idle");
    ASSERT_EQ(std::string(TASStateMachine::StateToString(TASStateMachine::State::PendingRecord)), "PendingRecord");
    ASSERT_EQ(std::string(TASStateMachine::StateToString(TASStateMachine::State::Recording)), "Recording");
    ASSERT_EQ(std::string(TASStateMachine::StateToString(TASStateMachine::State::PlayingScript)), "PlayingScript");
}

TEST(StateMachineTest, EventToString) {
    ASSERT_EQ(std::string(TASStateMachine::EventToString(TASStateMachine::Event::StartRecording)), "StartRecording");
    ASSERT_EQ(std::string(TASStateMachine::EventToString(TASStateMachine::Event::Stop)), "Stop");
    ASSERT_EQ(std::string(TASStateMachine::EventToString(TASStateMachine::Event::LevelLoadStart)), "LevelLoadStart");
    ASSERT_EQ(std::string(TASStateMachine::EventToString(TASStateMachine::Event::LevelStart)), "LevelStart");
    ASSERT_EQ(std::string(TASStateMachine::EventToString(TASStateMachine::Event::LevelEnd)), "LevelEnd");
}

TEST(TransitionTest, IdleToPendingRecord) {
    TASStateMachine sm(nullptr);

    auto idleHandler = std::make_unique<MockIdleHandler>();
    auto pendingHandler = std::make_unique<MockPendingRecordHandler>();
    auto *idlePtr = idleHandler.get();
    auto *pendingPtr = pendingHandler.get();

    sm.RegisterHandler(TASStateMachine::State::Idle, std::move(idleHandler));
    sm.RegisterHandler(TASStateMachine::State::PendingRecord, std::move(pendingHandler));

    auto result = sm.Transition(TASStateMachine::Event::StartRecording);
    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(sm.GetCurrentState(), TASStateMachine::State::PendingRecord);
    ASSERT_TRUE(sm.IsPending());
    ASSERT_TRUE(sm.IsPendingRecord());

    ASSERT_TRUE(idlePtr->exitCalled);
    ASSERT_TRUE(pendingPtr->enterCalled);
}

TEST(TransitionTest, PendingRecordToRecordingOnLevelLoadStart) {
    TASStateMachine sm(nullptr);

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::PendingRecord, std::make_unique<MockPendingRecordHandler>());
    sm.RegisterHandler(TASStateMachine::State::Recording, std::make_unique<MockRecordingHandler>());

    sm.Transition(TASStateMachine::Event::StartRecording);
    ASSERT_TRUE(sm.IsPendingRecord());

    auto result = sm.Transition(TASStateMachine::Event::LevelLoadStart);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(sm.IsRecording());
}

TEST(TransitionTest, PendingRecordPlaybackToPlayingRecordOnLevelLoadStart) {
    TASStateMachine sm(nullptr);

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::PendingRecordPlayback, std::make_unique<MockPendingRecordHandler>());
    sm.RegisterHandler(TASStateMachine::State::PlayingRecord, std::make_unique<MockPendingRecordHandler>());

    auto startResult = sm.Transition(TASStateMachine::Event::StartRecordPlayback);
    ASSERT_TRUE(startResult.IsOk());
    ASSERT_EQ(sm.GetCurrentState(), TASStateMachine::State::PendingRecordPlayback);

    auto result = sm.Transition(TASStateMachine::Event::LevelLoadStart);
    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(sm.GetCurrentState(), TASStateMachine::State::PlayingRecord);
}

TEST(TransitionTest, PendingScriptPlaybackToPlayingScriptOnLevelLoadStart) {
    TASStateMachine sm(nullptr);

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::PendingScriptPlayback, std::make_unique<MockPendingRecordHandler>());
    sm.RegisterHandler(TASStateMachine::State::PlayingScript, std::make_unique<MockPendingRecordHandler>());

    auto startResult = sm.Transition(TASStateMachine::Event::StartScriptPlayback);
    ASSERT_TRUE(startResult.IsOk());
    ASSERT_EQ(sm.GetCurrentState(), TASStateMachine::State::PendingScriptPlayback);

    auto result = sm.Transition(TASStateMachine::Event::LevelLoadStart);
    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(sm.GetCurrentState(), TASStateMachine::State::PlayingScript);
}

TEST(TransitionTest, InvalidTransition) {
    TASStateMachine sm(nullptr);

    auto result = sm.Transition(TASStateMachine::Event::Resume);
    ASSERT_TRUE(result.IsError());
    ASSERT_EQ(sm.GetCurrentState(), TASStateMachine::State::Idle);
}

TEST(TransitionTest, CancelPendingWithStop) {
    TASStateMachine sm(nullptr);

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::PendingRecord, std::make_unique<MockPendingRecordHandler>());

    sm.Transition(TASStateMachine::Event::StartRecording);
    ASSERT_TRUE(sm.IsPendingRecord());

    auto result = sm.Transition(TASStateMachine::Event::Stop);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(sm.IsIdle());
}

TEST(TransitionTest, StopTransition) {
    TASStateMachine sm(nullptr);

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::PendingRecord, std::make_unique<MockPendingRecordHandler>());
    sm.RegisterHandler(TASStateMachine::State::Recording, std::make_unique<MockRecordingHandler>());

    sm.Transition(TASStateMachine::Event::StartRecording);
    sm.Transition(TASStateMachine::Event::LevelLoadStart);
    ASSERT_TRUE(sm.IsRecording());

    auto result = sm.Transition(TASStateMachine::Event::Stop);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(sm.IsIdle());
}

TEST(TransitionTest, LevelEndStopsActive) {
    TASStateMachine sm(nullptr);

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::PendingRecord, std::make_unique<MockPendingRecordHandler>());
    sm.RegisterHandler(TASStateMachine::State::Recording, std::make_unique<MockRecordingHandler>());

    sm.Transition(TASStateMachine::Event::StartRecording);
    sm.Transition(TASStateMachine::Event::LevelLoadStart);
    ASSERT_TRUE(sm.IsRecording());

    auto result = sm.Transition(TASStateMachine::Event::LevelEnd);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(sm.IsIdle());
}

TEST(TransitionTest, MultipleTransitions) {
    TASStateMachine sm(nullptr);

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::PendingRecord, std::make_unique<MockPendingRecordHandler>());
    sm.RegisterHandler(TASStateMachine::State::Recording, std::make_unique<MockRecordingHandler>());

    sm.Transition(TASStateMachine::Event::StartRecording);
    sm.Transition(TASStateMachine::Event::LevelLoadStart);
    ASSERT_TRUE(sm.IsRecording());

    sm.Transition(TASStateMachine::Event::Stop);
    ASSERT_TRUE(sm.IsIdle());

    sm.Transition(TASStateMachine::Event::StartRecording);
    sm.Transition(TASStateMachine::Event::LevelLoadStart);
    ASSERT_TRUE(sm.IsRecording());
}

TEST(TransitionTest, ShutdownFromIdle) {
    TASStateMachine sm(nullptr);

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());

    auto result = sm.Transition(TASStateMachine::Event::Shutdown);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(sm.IsShuttingDown());
}

TEST(TransitionTest, ShutdownFromRecording) {
    TASStateMachine sm(nullptr);

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::PendingRecord, std::make_unique<MockPendingRecordHandler>());
    sm.RegisterHandler(TASStateMachine::State::Recording, std::make_unique<MockRecordingHandler>());

    sm.Transition(TASStateMachine::Event::StartRecording);
    sm.Transition(TASStateMachine::Event::LevelLoadStart);
    ASSERT_TRUE(sm.IsRecording());

    auto result = sm.Transition(TASStateMachine::Event::Shutdown);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(sm.IsShuttingDown());
}

TEST(HandlerTest, Blocking) {
    TASStateMachine sm(nullptr);

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::PendingRecord, std::make_unique<MockPendingRecordHandler>());
    sm.RegisterHandler(TASStateMachine::State::Recording, std::make_unique<MockRecordingHandler>());

    sm.Transition(TASStateMachine::Event::StartRecording);
    sm.Transition(TASStateMachine::Event::LevelLoadStart);
    ASSERT_TRUE(sm.IsRecording());

    auto result = sm.Transition(TASStateMachine::Event::Stop);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(sm.IsIdle());
}

TEST(HandlerTest, TickCalling) {
    TASStateMachine sm(nullptr);

    auto handler = std::make_unique<MockRecordingHandler>();
    auto *handlerPtr = handler.get();
    sm.RegisterHandler(TASStateMachine::State::Recording, std::move(handler));
    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::PendingRecord, std::make_unique<MockPendingRecordHandler>());

    sm.Transition(TASStateMachine::Event::StartRecording);
    sm.Transition(TASStateMachine::Event::LevelLoadStart);
    ASSERT_TRUE(sm.IsRecording());

    sm.Tick();
    ASSERT_TRUE(handlerPtr->tickCalled);
}

TEST(HistoryTest, TransitionHistory) {
    TASStateMachine sm(nullptr);

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::PendingRecord, std::make_unique<MockPendingRecordHandler>());
    sm.RegisterHandler(TASStateMachine::State::Recording, std::make_unique<MockRecordingHandler>());

    sm.Transition(TASStateMachine::Event::StartRecording);
    sm.Transition(TASStateMachine::Event::LevelLoadStart);
    sm.Transition(TASStateMachine::Event::Stop);

    const auto &history = sm.GetTransitionHistory();
    ASSERT_EQ(history.size(), 3);

    ASSERT_EQ(history[0].fromState, TASStateMachine::State::Idle);
    ASSERT_EQ(history[0].event, TASStateMachine::Event::StartRecording);
    ASSERT_EQ(history[0].toState, TASStateMachine::State::PendingRecord);
    ASSERT_TRUE(history[0].succeeded);

    ASSERT_EQ(history[1].fromState, TASStateMachine::State::PendingRecord);
    ASSERT_EQ(history[1].event, TASStateMachine::Event::LevelLoadStart);
    ASSERT_EQ(history[1].toState, TASStateMachine::State::Recording);
    ASSERT_TRUE(history[1].succeeded);

    ASSERT_EQ(history[2].fromState, TASStateMachine::State::Recording);
    ASSERT_EQ(history[2].event, TASStateMachine::Event::Stop);
    ASSERT_EQ(history[2].toState, TASStateMachine::State::Idle);
    ASSERT_TRUE(history[2].succeeded);
}

TEST(HistoryTest, ClearHistory) {
    TASStateMachine sm(nullptr);

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::PendingRecord, std::make_unique<MockPendingRecordHandler>());

    sm.Transition(TASStateMachine::Event::StartRecording);
    ASSERT_EQ(sm.GetTransitionHistory().size(), 1);

    sm.ClearHistory();
    ASSERT_EQ(sm.GetTransitionHistory().size(), 0);
}

TEST(ForceSetStateTest, ForceSetState) {
    TASStateMachine sm(nullptr);

    sm.RegisterHandler(TASStateMachine::State::Idle, std::make_unique<MockIdleHandler>());
    sm.RegisterHandler(TASStateMachine::State::Recording, std::make_unique<MockRecordingHandler>());

    auto result = sm.ForceSetState(TASStateMachine::State::Recording);
    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(sm.IsRecording());
}
