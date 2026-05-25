#include <gtest/gtest.h>

#include "Runtime/RuntimeSession.h"

TEST(RuntimeSessionStateTest, StartsIdle) {
    RuntimeSession session;

    auto snapshot = session.Snapshot();
    EXPECT_EQ(snapshot.state, RuntimeSession::State::Idle);
    EXPECT_TRUE(snapshot.isIdle);
    EXPECT_FALSE(snapshot.isRecording);
    EXPECT_FALSE(snapshot.isPlaying);
    EXPECT_FALSE(snapshot.isPending);
}

TEST(RuntimeSessionStateTest, RecordingMovesFromPendingToActiveOnLevelLoad) {
    RuntimeSession session;

    auto start = session.StartRecording({});
    ASSERT_TRUE(start.IsOk()) << start.GetError().message;
    EXPECT_EQ(session.Snapshot().state, RuntimeSession::State::PendingRecord);
    EXPECT_TRUE(session.Snapshot().isPendingRecord);

    auto activate = session.OnLevelLoadStart();
    ASSERT_TRUE(activate.IsOk()) << activate.GetError().message;
    EXPECT_EQ(session.Snapshot().state, RuntimeSession::State::Recording);
    EXPECT_TRUE(session.Snapshot().isRecording);
}

TEST(RuntimeSessionStateTest, PlaybackMovesToMatchingActiveStateOnLevelLoad) {
    RuntimeSession scriptSession;
    ASSERT_TRUE(scriptSession.StartPlayback(nullptr, PlaybackType::Script, {}).IsOk());
    ASSERT_TRUE(scriptSession.OnLevelLoadStart().IsOk());
    EXPECT_EQ(scriptSession.Snapshot().state, RuntimeSession::State::PlayingScript);
    EXPECT_EQ(scriptSession.Snapshot().playbackType, PlaybackType::Script);

    RuntimeSession recordSession;
    ASSERT_TRUE(recordSession.StartPlayback(nullptr, PlaybackType::Record, {}).IsOk());
    ASSERT_TRUE(recordSession.OnLevelLoadStart().IsOk());
    EXPECT_EQ(recordSession.Snapshot().state, RuntimeSession::State::PlayingRecord);
    EXPECT_EQ(recordSession.Snapshot().playbackType, PlaybackType::Record);
}

TEST(RuntimeSessionStateTest, PauseAndResumeReturnToPreviousPlaybackState) {
    RuntimeSession session;
    ASSERT_TRUE(session.StartPlayback(nullptr, PlaybackType::Script, {}).IsOk());
    ASSERT_TRUE(session.OnLevelLoadStart().IsOk());

    ASSERT_TRUE(session.Pause().IsOk());
    EXPECT_EQ(session.Snapshot().state, RuntimeSession::State::Paused);
    EXPECT_TRUE(session.Snapshot().isPaused);
    EXPECT_EQ(session.Snapshot().playbackType, PlaybackType::Script);

    ASSERT_TRUE(session.Resume().IsOk());
    EXPECT_EQ(session.Snapshot().state, RuntimeSession::State::PlayingScript);
    EXPECT_TRUE(session.Snapshot().isPlaying);
}

TEST(RuntimeSessionStateTest, StopAndLevelEndReturnToIdle) {
    RuntimeSession stopSession;
    ASSERT_TRUE(stopSession.StartRecording({}).IsOk());
    ASSERT_TRUE(stopSession.OnLevelLoadStart().IsOk());
    ASSERT_TRUE(stopSession.Stop({}).IsOk());
    EXPECT_EQ(stopSession.Snapshot().state, RuntimeSession::State::Idle);

    RuntimeSession levelEndSession;
    ASSERT_TRUE(levelEndSession.StartTranslation(nullptr, {}).IsOk());
    ASSERT_TRUE(levelEndSession.OnLevelLoadStart().IsOk());
    ASSERT_TRUE(levelEndSession.OnLevelEnd({}).IsOk());
    EXPECT_EQ(levelEndSession.Snapshot().state, RuntimeSession::State::Idle);
}

TEST(RuntimeSessionStateTest, RecordsTransitionHistory) {
    RuntimeSession session;

    ASSERT_TRUE(session.StartRecording({}).IsOk());
    ASSERT_TRUE(session.OnLevelLoadStart().IsOk());
    ASSERT_TRUE(session.Stop({}).IsOk());

    const auto &history = session.GetTransitionHistory();
    ASSERT_EQ(history.size(), 3);
    EXPECT_EQ(history[0].fromState, RuntimeSession::State::Idle);
    EXPECT_EQ(history[0].event, RuntimeSession::Event::StartRecording);
    EXPECT_EQ(history[0].toState, RuntimeSession::State::PendingRecord);
    EXPECT_TRUE(history[0].succeeded);
    EXPECT_EQ(history[2].toState, RuntimeSession::State::Idle);
}

TEST(RuntimeSessionStateTest, InvalidTransitionReturnsError) {
    RuntimeSession session;

    auto result = session.Resume();
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(session.Snapshot().state, RuntimeSession::State::Idle);
}
