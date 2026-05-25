#include <gtest/gtest.h>

#include "Runtime/RuntimeSession.h"

namespace {

TASProject *FakeProject() {
    return reinterpret_cast<TASProject *>(0x1);
}

} // namespace

TEST(RuntimeSessionLifecycleTest, RecordingPrepareActivateStopLifecycle) {
    RuntimeSession::Hooks hooks;
    bool prepared = false;
    bool active = false;
    int prepareCount = 0;
    int activateCount = 0;
    int gracefulStopCount = 0;

    hooks.prepareRecording = [&](RuntimeSession::RecordingOptions) {
        prepared = true;
        ++prepareCount;
        return Result<void>::Ok();
    };
    hooks.activateRecording = [&]() {
        active = true;
        ++activateCount;
        return Result<void>::Ok();
    };
    hooks.stopRecordingGraceful = [&]() {
        prepared = false;
        active = false;
        ++gracefulStopCount;
        return Result<void>::Ok();
    };
    hooks.isRecordingPrepared = [&]() { return prepared; };
    hooks.isRecordingActive = [&]() { return active; };

    RuntimeSession session(nullptr, hooks);

    ASSERT_TRUE(session.StartRecording({false}).IsOk());
    ASSERT_TRUE(session.OnLevelLoadStart().IsOk());
    ASSERT_TRUE(session.Stop({false}).IsOk());

    EXPECT_EQ(prepareCount, 1);
    EXPECT_EQ(activateCount, 1);
    EXPECT_EQ(gracefulStopCount, 1);
    EXPECT_TRUE(session.IsIdle());
}

TEST(RuntimeSessionLifecycleTest, PlaybackStopReleasesHookLeases) {
    RuntimeSession::Hooks hooks;
    bool prepared = false;
    bool active = false;
    bool postTickLease = false;
    bool postInputLease = false;

    hooks.preparePlayback = [&](TASProject *, PlaybackType) {
        prepared = true;
        return Result<void>::Ok();
    };
    hooks.activatePlayback = [&]() {
        active = true;
        postTickLease = true;
        postInputLease = true;
        return Result<void>::Ok();
    };
    hooks.stopPlaybackGraceful = [&](bool) {
        prepared = false;
        active = false;
        postTickLease = false;
        postInputLease = false;
        return Result<void>::Ok();
    };
    hooks.isPlaybackPrepared = [&]() { return prepared; };
    hooks.isPlaybackActiveOrPaused = [&]() { return active; };

    RuntimeSession session(nullptr, hooks);

    ASSERT_TRUE(session.StartPlayback(FakeProject(), PlaybackType::Script, {false}).IsOk());
    ASSERT_TRUE(session.OnLevelLoadStart().IsOk());
    ASSERT_TRUE(postTickLease);
    ASSERT_TRUE(postInputLease);

    ASSERT_TRUE(session.Stop({true}).IsOk());
    EXPECT_FALSE(postTickLease);
    EXPECT_FALSE(postInputLease);
}

TEST(RuntimeSessionLifecycleTest, TranslationShutdownUsesImmediateCleanup) {
    RuntimeSession::Hooks hooks;
    bool prepared = false;
    bool active = false;
    int immediateStopCount = 0;

    hooks.prepareTranslation = [&](TASProject *) {
        prepared = true;
        return Result<void>::Ok();
    };
    hooks.activateTranslation = [&]() {
        active = true;
        return Result<void>::Ok();
    };
    hooks.stopTranslationImmediate = [&]() {
        prepared = false;
        active = false;
        ++immediateStopCount;
    };
    hooks.isTranslationPrepared = [&]() { return prepared; };
    hooks.isTranslationActive = [&]() { return active; };

    RuntimeSession session(nullptr, hooks);

    ASSERT_TRUE(session.StartTranslation(FakeProject(), {}).IsOk());
    ASSERT_TRUE(session.OnLevelLoadStart().IsOk());
    ASSERT_TRUE(session.Shutdown().IsOk());

    EXPECT_EQ(immediateStopCount, 1);
    EXPECT_TRUE(session.IsShuttingDown());
}

TEST(RuntimeSessionLifecycleTest, ValidationOptionControlsStartAndGracefulStop) {
    RuntimeSession::Hooks hooks;
    bool prepared = false;
    bool active = false;
    bool validationActive = false;
    int validationStartCount = 0;
    int validationStopCount = 0;

    hooks.preparePlayback = [&](TASProject *, PlaybackType) {
        prepared = true;
        return Result<void>::Ok();
    };
    hooks.activatePlayback = [&]() {
        active = true;
        return Result<void>::Ok();
    };
    hooks.stopPlaybackGraceful = [&](bool) {
        prepared = false;
        active = false;
        return Result<void>::Ok();
    };
    hooks.isPlaybackPrepared = [&]() { return prepared; };
    hooks.isPlaybackActiveOrPaused = [&]() { return active; };
    hooks.currentPlaybackProject = [&]() { return FakeProject(); };
    hooks.startValidationForPlayback = [&](TASProject *) {
        validationActive = true;
        ++validationStartCount;
        return Result<void>::Ok();
    };
    hooks.stopValidationGraceful = [&]() {
        validationActive = false;
        ++validationStopCount;
        return Result<void>::Ok();
    };
    hooks.isValidationActive = [&]() { return validationActive; };

    RuntimeSession disabledSession(nullptr, hooks);
    ASSERT_TRUE(disabledSession.StartPlayback(FakeProject(), PlaybackType::Script, {false}).IsOk());
    ASSERT_TRUE(disabledSession.OnLevelLoadStart().IsOk());
    ASSERT_TRUE(disabledSession.OnLevelStart().IsOk());
    ASSERT_TRUE(disabledSession.Stop({false}).IsOk());
    EXPECT_EQ(validationStartCount, 0);
    EXPECT_EQ(validationStopCount, 0);

    RuntimeSession enabledSession(nullptr, hooks);
    ASSERT_TRUE(enabledSession.StartPlayback(FakeProject(), PlaybackType::Script, {true}).IsOk());
    ASSERT_TRUE(enabledSession.OnLevelLoadStart().IsOk());
    ASSERT_TRUE(enabledSession.OnLevelStart().IsOk());
    ASSERT_TRUE(enabledSession.Stop({false}).IsOk());
    EXPECT_EQ(validationStartCount, 1);
    EXPECT_EQ(validationStopCount, 1);
}

TEST(RuntimeSessionLifecycleTest, ShutdownStopsValidationImmediately) {
    RuntimeSession::Hooks hooks;
    bool prepared = false;
    bool active = false;
    bool validationActive = false;
    int validationImmediateStopCount = 0;

    hooks.preparePlayback = [&](TASProject *, PlaybackType) {
        prepared = true;
        return Result<void>::Ok();
    };
    hooks.activatePlayback = [&]() {
        active = true;
        return Result<void>::Ok();
    };
    hooks.stopPlaybackImmediate = [&]() {
        prepared = false;
        active = false;
    };
    hooks.isPlaybackPrepared = [&]() { return prepared; };
    hooks.isPlaybackActiveOrPaused = [&]() { return active; };
    hooks.currentPlaybackProject = [&]() { return FakeProject(); };
    hooks.startValidationForPlayback = [&](TASProject *) {
        validationActive = true;
        return Result<void>::Ok();
    };
    hooks.stopValidationImmediate = [&]() {
        validationActive = false;
        ++validationImmediateStopCount;
    };
    hooks.isValidationActive = [&]() { return validationActive; };

    RuntimeSession session(nullptr, hooks);
    ASSERT_TRUE(session.StartPlayback(FakeProject(), PlaybackType::Script, {true}).IsOk());
    ASSERT_TRUE(session.OnLevelLoadStart().IsOk());
    ASSERT_TRUE(session.OnLevelStart().IsOk());
    ASSERT_TRUE(session.Shutdown().IsOk());

    EXPECT_EQ(validationImmediateStopCount, 1);
    EXPECT_TRUE(session.IsShuttingDown());
}
