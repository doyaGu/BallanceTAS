#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ScriptGenerator.h"
#include "ScriptGenerationCore.h"
#include "ScriptInputTransition.h"

namespace {

RawInputState State(bool up, bool right) {
    RawInputState state;
    state.keyUp = up ? 1 : KS_IDLE;
    state.keyRight = right ? 1 : KS_IDLE;
    return state;
}

RawInputState RawState(uint8_t up, uint8_t right = KS_IDLE) {
    RawInputState state;
    state.keyUp = up;
    state.keyRight = right;
    return state;
}

std::vector<KeyEvent> DetectAll(const std::vector<RawInputState> &states) {
    std::vector<KeyEvent> allEvents;
    RawInputState previous;
    for (size_t frame = 0; frame < states.size(); ++frame) {
        auto events = DetectScriptKeyTransitions(previous, states[frame], frame);
        allEvents.insert(allEvents.end(), events.begin(), events.end());
        previous = states[frame];
    }
    return allEvents;
}

TEST(ScriptGeneratorTest, PreservesCkEdgeReleaseWithoutDuplicateIdleRelease) {
    auto events = DetectAll({
        RawState(KS_IDLE),
        RawState(KS_PRESSED),
        RawState(KS_PRESSED | KS_RELEASED),
        RawState(KS_IDLE),
    });

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].key, "up");
    EXPECT_EQ(events[0].transition, KeyTransition::Pressed);
    EXPECT_EQ(events[1].key, "up");
    EXPECT_EQ(events[1].transition, KeyTransition::Released);
}

TEST(ScriptGeneratorTest, PreservesCkSingleFramePressAndRelease) {
    auto events = DetectAll({
        RawState(KS_IDLE),
        RawState(KS_PRESSED | KS_RELEASED),
        RawState(KS_IDLE),
    });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].key, "up");
    EXPECT_EQ(events[0].transition, KeyTransition::PressedAndReleased);
}

size_t Count(const std::vector<KeyEvent> &events, const char *key, KeyTransition transition) {
    size_t count = 0;
    for (const auto &event : events) {
        if (event.key == key && event.transition == transition) {
            ++count;
        }
    }
    return count;
}

} // namespace

TEST(ScriptGeneratorTest, TranslatesLegacyDownStateFramesIntoBalancedKeyEdges) {
    auto events = DetectAll({
        State(false, false),
        State(true, true),
        State(true, true),
        State(true, false),
        State(false, false),
    });

    EXPECT_EQ(Count(events, "up", KeyTransition::Pressed), 1u);
    EXPECT_EQ(Count(events, "up", KeyTransition::Released), 1u);
    EXPECT_EQ(Count(events, "right", KeyTransition::Pressed), 1u);
    EXPECT_EQ(Count(events, "right", KeyTransition::Released), 1u);

    ASSERT_EQ(events.size(), 4u);
    EXPECT_EQ(events[0].key, "up");
    EXPECT_EQ(events[0].transition, KeyTransition::Pressed);
    EXPECT_EQ(events[1].key, "right");
    EXPECT_EQ(events[1].transition, KeyTransition::Pressed);
    EXPECT_EQ(events[2].key, "right");
    EXPECT_EQ(events[2].transition, KeyTransition::Released);
    EXPECT_EQ(events[3].key, "up");
    EXPECT_EQ(events[3].transition, KeyTransition::Released);
}

TEST(ScriptGeneratorTest, ReleasesLegacyDownStateOnLastPressedFrame) {
    auto events = DetectAll({
        State(false, false),
        State(true, false),
        State(true, false),
        State(false, false),
    });

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].key, "up");
    EXPECT_EQ(events[0].transition, KeyTransition::Pressed);
    EXPECT_EQ(events[0].frame, 1u);
    EXPECT_EQ(events[1].key, "up");
    EXPECT_EQ(events[1].transition, KeyTransition::Released);
    EXPECT_EQ(events[1].frame, 2u);
}

TEST(ScriptGeneratorTest, GeneratedScriptCollapsesSingleFrameLegacyPress) {
    std::vector<FrameData> frames(3);
    frames[0].frameIndex = 0;
    frames[1].frameIndex = 1;
    frames[1].inputState.keyUp = KS_PRESSED;
    frames[2].frameIndex = 2;

    GenerationOptions options;
    options.addFrameComments = false;
    options.addSectionSeparators = false;

    const std::string script = BuildGeneratedLuaScriptForTesting(frames, options);
    EXPECT_NE(script.find("tas.press(\"up\")"), std::string::npos) << script;
    EXPECT_EQ(script.find("tas.key_down(\"up\")"), std::string::npos) << script;
    EXPECT_EQ(script.find("tas.key_up(\"up\")"), std::string::npos) << script;
}

TEST(ScriptGeneratorTest, GeneratedScriptDoesNotRestoreRngInsideMainLua) {
    std::vector<FrameData> frames(3);
    frames[0].frameIndex = 0;
    frames[1].frameIndex = 1;
    frames[1].inputState.keyUp = KS_PRESSED;
    frames[2].frameIndex = 2;

    GenerationOptions options;
    options.addFrameComments = false;
    options.addSectionSeparators = false;

    const std::string script = BuildGeneratedLuaScriptForTesting(frames, options);
    const std::string removedApi = std::string("tas.") + "rng.set_state";
    EXPECT_EQ(script.find(removedApi), std::string::npos) << script;
}

TEST(ScriptGeneratorTest, GeneratedScriptDoesNotRestoreEventRngAtScriptTick) {
    std::vector<FrameData> frames(4);
    for (size_t i = 0; i < frames.size(); ++i) {
        frames[i].frameIndex = i;
    }
    frames[2].events.emplace_back(2, "pre_checkpoint_reached", 3);
    frames[3].inputState.keyRight = KS_PRESSED;

    GenerationOptions options;
    options.addFrameComments = false;
    options.addEventAnchors = true;
    options.addSectionSeparators = false;

    const std::string script = BuildGeneratedLuaScriptForTesting(frames, options);
    const size_t waitPos = script.find("tas.wait_ticks(2)");
    const std::string removedApi = std::string("tas.") + "rng.set_state({ id = 8, next_movement_check = 12, ivp_seed = 222, qh_seed = 333 })";
    const size_t rngPos = script.find(removedApi);
    const size_t eventPos = script.find("GAME EVENT: pre_checkpoint_reached");
    const size_t inputPos = script.find("tas.key_down(\"right\")");

    ASSERT_NE(waitPos, std::string::npos) << script;
    EXPECT_EQ(rngPos, std::string::npos) << script;
    ASSERT_NE(eventPos, std::string::npos) << script;
    ASSERT_NE(inputPos, std::string::npos) << script;
    EXPECT_LT(waitPos, eventPos);
    EXPECT_LT(eventPos, inputPos);
}

TEST(ScriptGeneratorTest, GeneratedManifestDoesNotStorePreloadRngState) {
    GenerationOptions options;
    options.projectName = "RngReplay";
    options.authorName = "Tester";
    options.targetLevel = "Level_02";

    ScriptGenerationStatsView stats;
    stats.totalFrames = 3;
    stats.totalBlocks = 2;
    stats.keyEvents = 9;
    const std::string manifest = BuildGeneratedManifestForTesting(options, stats);

    const std::string removedField = std::string("preload_") + "rng_state";
    EXPECT_EQ(manifest.find(removedField), std::string::npos) << manifest;
    EXPECT_EQ(manifest.find("next_movement_check"), std::string::npos) << manifest;
    EXPECT_EQ(manifest.find("ivp_seed"), std::string::npos) << manifest;
    EXPECT_EQ(manifest.find("qh_seed"), std::string::npos) << manifest;
}
