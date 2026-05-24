#include "TASMenuPresentation.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <CKGlobals.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "RecordPlayer.h"
#include "TASProject.h"

using ::testing::HasSubstr;
using ::testing::Not;

static tas::lua::LuaValue MakeValue(std::string value) {
    return tas::lua::LuaValue(tas::lua::LuaValue::Storage{std::move(value)});
}

static tas::lua::LuaValue MakeValue(double value) {
    return tas::lua::LuaValue(tas::lua::LuaValue::Storage{static_cast<lua_Number>(value)});
}

static tas::lua::LuaValue MakeManifest(std::string name,
                                std::string scope,
                                std::string trigger,
                                std::string level = "Level_01",
                                std::string entry = "main.lua") {
    auto table = std::make_shared<tas::lua::LuaValue::Table>();
    auto add = [&](std::string key, tas::lua::LuaValue value) {
        table->entries.push_back({
            tas::lua::LuaValue::Key{std::move(key)},
            std::make_shared<tas::lua::LuaValue>(std::move(value)),
        });
    };

    add("name", MakeValue(std::move(name)));
    add("author", MakeValue("Tester"));
    add("scope", MakeValue(std::move(scope)));
    add("trigger", MakeValue(std::move(trigger)));
    add("level", MakeValue(std::move(level)));
    add("entry_script", MakeValue(std::move(entry)));
    add("update_rate", MakeValue(132.0));
    return tas::lua::LuaValue(tas::lua::LuaValue::Storage{std::move(table)});
}

static tas::lua::LuaValue MakeManifestWithLegacyPreloadRng() {
    auto table = std::make_shared<tas::lua::LuaValue::Table>();
    auto add = [&](std::string key, tas::lua::LuaValue value) {
        table->entries.push_back({
            tas::lua::LuaValue::Key{std::move(key)},
            std::make_shared<tas::lua::LuaValue>(std::move(value)),
        });
    };

    add("name", MakeValue("RngScript"));
    add("author", MakeValue("Tester"));
    add("scope", MakeValue("level"));
    add("trigger", MakeValue("level"));
    add("level", MakeValue("Level_02"));
    add("entry_script", MakeValue("main.lua"));
    add("update_rate", MakeValue(132.0));

    auto rng = std::make_shared<tas::lua::LuaValue::Table>();
    auto addRng = [&](std::string key, lua_Integer value) {
        rng->entries.push_back({
            tas::lua::LuaValue::Key{std::move(key)},
            std::make_shared<tas::lua::LuaValue>(tas::lua::LuaValue::Storage{value}),
        });
    };
    addRng("id", 5);
    addRng("next_movement_check", 19);
    addRng("ivp_seed", -7654321);
    addRng("qh_seed", 44);
    add(std::string("preload_") + "rng_state", tas::lua::LuaValue(tas::lua::LuaValue::Storage{std::move(rng)}));

    return tas::lua::LuaValue(tas::lua::LuaValue::Storage{std::move(table)});
}

static std::filesystem::path TempRecordPath(const std::string &name) {
    auto path = std::filesystem::temp_directory_path() / "BallanceTAS_MenuPresentation";
    std::filesystem::create_directories(path);
    return path / name;
}

static void WriteZeroFrameRecord(const std::filesystem::path &path) {
    std::ofstream file(path, std::ios::binary);
    const uint32_t uncompressedSize = 0;
    file.write(reinterpret_cast<const char *>(&uncompressedSize), sizeof(uncompressedSize));
}

static void WriteInvalidHeaderRecord(const std::filesystem::path &path) {
    std::ofstream file(path, std::ios::binary);
    const uint16_t shortHeader = 1;
    file.write(reinterpret_cast<const char *>(&shortHeader), sizeof(shortHeader));
}

static void WritePackedRecord(const std::filesystem::path &path, const std::vector<RecordFrameData> &frames) {
    const auto uncompressedSize = static_cast<uint32_t>(frames.size() * sizeof(RecordFrameData));
    int compressedSize = 0;
    char *compressed = CKPackData(
        const_cast<char *>(reinterpret_cast<const char *>(frames.data())),
        static_cast<int>(uncompressedSize),
        compressedSize,
        9);
    ASSERT_NE(compressed, nullptr);
    ASSERT_GT(compressedSize, 0);

    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(&uncompressedSize), sizeof(uncompressedSize));
    file.write(compressed, compressedSize);
    CKDeletePointer(compressed);
}

static std::vector<RecordFrameData> MakeRecordFrames(size_t count, float firstDelta, float secondDelta) {
    std::vector<RecordFrameData> frames;
    frames.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        frames.emplace_back(i == 1 ? secondDelta : firstDelta);
    }
    return frames;
}

TEST(TASMenuPresentationTest, BuildsNativeScriptLabelsAndSafeTruncation) {
    TASProject project("C:/TAS/LongProject",
                       MakeManifest("ThisIsAnExtremelyLongProjectNameThatMustNotOverflow", "global", "menu", ""));

    auto state = BuildMenuStatePresentation(TASMenuRuntimeSnapshot{});
    auto presentation = BuildProjectPresentation(project, state);

    EXPECT_EQ(presentation.typeLabel, "SCRIPT");
    EXPECT_EQ(presentation.scopeLabel, "GLOBAL");
    EXPECT_EQ(presentation.triggerLabel, "menu");
    EXPECT_EQ(presentation.fullName, "ThisIsAnExtremelyLongProjectNameThatMustNotOverflow");
    EXPECT_LT(presentation.displayName.size(), presentation.fullName.size());
    EXPECT_THAT(presentation.displayName, HasSubstr("..."));
    EXPECT_EQ(presentation.rowStatusLabel, "GLOBAL");
    EXPECT_EQ(presentation.rowBadgeLabel, "G");
    EXPECT_TRUE(presentation.playAction.enabled);
    EXPECT_TRUE(presentation.playAction.disabledReason.empty());
}

TEST(TASMenuPresentationTest, ShowsLevelScriptTargetAndMissingEntryReason) {
    TASProject project("C:/TAS/LevelScript",
                       MakeManifest("LevelScript", "level", "level", "Level_03", ""));

    auto presentation = BuildProjectPresentation(project, BuildMenuStatePresentation(TASMenuRuntimeSnapshot{}));

    EXPECT_EQ(presentation.scopeLabel, "LEVEL:Level_03");
    EXPECT_EQ(presentation.levelLabel, "Level_03");
    EXPECT_FALSE(presentation.playAction.enabled);
    EXPECT_THAT(presentation.playAction.disabledReason, HasSubstr("entry"));
    EXPECT_LE(presentation.rowBadgeLabel.size(), 3u);
}

TEST(TASMenuPresentationTest, ExposesZeroFrameRecordAsUnplayableUserReason) {
    auto path = TempRecordPath("empty_record.tas");
    WriteZeroFrameRecord(path);

    TASProject project(path.string());

    EXPECT_EQ(project.GetRecordFrameCount(), 0u);
    EXPECT_FALSE(project.IsValid());
    EXPECT_FALSE(project.CanPlayRecord());
    EXPECT_THAT(project.GetValidationMessage(), HasSubstr("no frames"));

    auto presentation = BuildProjectPresentation(project, BuildMenuStatePresentation(TASMenuRuntimeSnapshot{}));
    EXPECT_EQ(presentation.typeLabel, "RECORD");
    EXPECT_FALSE(presentation.playAction.enabled);
    EXPECT_THAT(presentation.playAction.disabledReason, HasSubstr("no frames"));
    EXPECT_FALSE(presentation.translateAction.enabled);
}

TEST(TASMenuPresentationTest, ExposesInvalidRecordHeaderReason) {
    auto path = TempRecordPath("invalid_header.tas");
    WriteInvalidHeaderRecord(path);

    TASProject project(path.string());

    EXPECT_EQ(project.GetRecordFrameCount(), 0u);
    EXPECT_FALSE(project.IsValid());
    EXPECT_FALSE(project.CanPlayRecord());
    EXPECT_THAT(project.GetValidationMessage(), HasSubstr("header"));

    auto presentation = BuildProjectPresentation(project, BuildMenuStatePresentation(TASMenuRuntimeSnapshot{}));
    EXPECT_FALSE(presentation.playAction.enabled);
    EXPECT_THAT(presentation.playAction.disabledReason, HasSubstr("header"));
}

TEST(TASMenuPresentationTest, ReportsRecordTranslationCompatibility) {
    auto constantPath = TempRecordPath("constant_record.tas");
    WritePackedRecord(constantPath, MakeRecordFrames(32, 1000.0f / 132.0f, 1000.0f / 132.0f));

    TASProject constantRecord(constantPath.string());
    EXPECT_TRUE(constantRecord.IsValid());
    EXPECT_TRUE(constantRecord.CanPlayRecord());
    EXPECT_EQ(constantRecord.GetRecordFrameCount(), 32u);
    EXPECT_TRUE(constantRecord.CanBeTranslated());
    EXPECT_THAT(constantRecord.GetTranslationCompatibilityMessage(), HasSubstr("constant"));

    auto constantPresentation = BuildProjectPresentation(constantRecord, BuildMenuStatePresentation(TASMenuRuntimeSnapshot{}));
    EXPECT_TRUE(constantPresentation.playAction.enabled);
    EXPECT_TRUE(constantPresentation.translateAction.enabled);

    auto variablePath = TempRecordPath("variable_record.tas");
    WritePackedRecord(variablePath, MakeRecordFrames(32, 1000.0f / 132.0f, 10.0f));

    TASProject variableRecord(variablePath.string());
    EXPECT_TRUE(variableRecord.IsValid());
    EXPECT_TRUE(variableRecord.CanPlayRecord());
    EXPECT_EQ(variableRecord.GetRecordFrameCount(), 32u);
    EXPECT_FALSE(variableRecord.CanBeTranslated());
    EXPECT_THAT(variableRecord.GetTranslationCompatibilityMessage(), HasSubstr("Variable timing"));

    auto variablePresentation = BuildProjectPresentation(variableRecord, BuildMenuStatePresentation(TASMenuRuntimeSnapshot{}));
    EXPECT_TRUE(variablePresentation.playAction.enabled);
    EXPECT_FALSE(variablePresentation.translateAction.enabled);
    EXPECT_THAT(variablePresentation.translateAction.disabledReason, HasSubstr("Variable timing"));
}

TEST(TASMenuPresentationTest, PresentsRecordAndScriptPlaybackWithConsistentTasSemantics) {
    TASProject script("C:/TAS/LevelScript",
                      MakeManifest("LevelScript", "level", "level", "Level_03"));

    auto recordPath = TempRecordPath("level_record.tas");
    WritePackedRecord(recordPath, MakeRecordFrames(16, 1000.0f / 132.0f, 1000.0f / 132.0f));
    TASProject record(recordPath.string());

    const auto state = BuildMenuStatePresentation(TASMenuRuntimeSnapshot{});
    const auto scriptPresentation = BuildProjectPresentation(script, state);
    const auto recordPresentation = BuildProjectPresentation(record, state);

    EXPECT_EQ(scriptPresentation.playAction.label, "Play TAS");
    EXPECT_EQ(recordPresentation.playAction.label, "Play TAS");
    EXPECT_EQ(scriptPresentation.rowStatusLabel, "LEVEL");
    EXPECT_EQ(recordPresentation.rowStatusLabel, "LEVEL");
    EXPECT_EQ(scriptPresentation.rowBadgeLabel, "L");
    EXPECT_EQ(recordPresentation.rowBadgeLabel, "L");
    EXPECT_TRUE(scriptPresentation.playAction.enabled);
    EXPECT_TRUE(recordPresentation.playAction.enabled);
    EXPECT_EQ(recordPresentation.translateAction.label, "Translate to Script");
    EXPECT_EQ(scriptPresentation.translateAction.label, "Translate to Record");
    EXPECT_TRUE(scriptPresentation.translateAction.enabled);
}

TEST(TASMenuPresentationTest, DisablesScriptToRecordTranslationForGlobalScripts) {
    TASProject globalScript("C:/TAS/GlobalScript",
                            MakeManifest("GlobalScript", "global", "startup", "", "main.lua"));

    const auto state = BuildMenuStatePresentation(TASMenuRuntimeSnapshot{});
    const auto presentation = BuildProjectPresentation(globalScript, state);

    EXPECT_EQ(presentation.translateAction.label, "Translate to Record");
    EXPECT_FALSE(presentation.translateAction.enabled);
    EXPECT_THAT(presentation.translateAction.disabledReason, HasSubstr("Only level script projects"));
}

TEST(TASMenuPresentationTest, BlocksRecordAndScriptPlaybackWithSameActiveTasReason) {
    TASMenuRuntimeSnapshot snapshot;
    snapshot.runningPlayback = true;
    snapshot.activeProjectName = "OtherTas";
    const auto state = BuildMenuStatePresentation(snapshot);

    TASProject script("C:/TAS/LevelScript",
                      MakeManifest("LevelScript", "level", "level", "Level_03"));

    auto recordPath = TempRecordPath("blocked_record.tas");
    WritePackedRecord(recordPath, MakeRecordFrames(16, 1000.0f / 132.0f, 1000.0f / 132.0f));
    TASProject record(recordPath.string());

    const auto scriptPresentation = BuildProjectPresentation(script, state);
    const auto recordPresentation = BuildProjectPresentation(record, state);

    EXPECT_FALSE(scriptPresentation.playAction.enabled);
    EXPECT_FALSE(recordPresentation.playAction.enabled);
    EXPECT_EQ(scriptPresentation.playAction.disabledReason, "Stop current TAS first");
    EXPECT_EQ(recordPresentation.playAction.disabledReason, "Stop current TAS first");
    EXPECT_EQ(scriptPresentation.rowStatusLabel, "LEVEL");
    EXPECT_EQ(recordPresentation.rowStatusLabel, "LEVEL");
}

TEST(TASMenuPresentationTest, UsesStableProjectKeyForActiveRowState) {
    TASProject script("C:/TAS/DuplicateScript",
                      MakeManifest("Duplicate", "level", "level", "Level_03"));

    auto recordPath = TempRecordPath("Duplicate.tas");
    WritePackedRecord(recordPath, MakeRecordFrames(16, 1000.0f / 132.0f, 1000.0f / 132.0f));
    TASProject record(recordPath.string());

    TASMenuRuntimeSnapshot snapshot;
    snapshot.runningPlayback = true;
    snapshot.activeProjectName = "Duplicate";
    snapshot.activeProjectKey = record.GetPath();
    const auto state = BuildMenuStatePresentation(snapshot);

    const auto scriptPresentation = BuildProjectPresentation(script, state);
    const auto recordPresentation = BuildProjectPresentation(record, state);

    EXPECT_EQ(scriptPresentation.rowStatusLabel, "LEVEL");
    EXPECT_EQ(scriptPresentation.rowBadgeLabel, "L");
    EXPECT_EQ(recordPresentation.rowStatusLabel, "RUN");
    EXPECT_EQ(recordPresentation.rowBadgeLabel, "RUN");
}

TEST(TASMenuPresentationTest, DoesNotShowProjectTypeMarkerInListRows) {
    TASProject script("C:/TAS/ScriptProject",
                      MakeManifest("ScriptProject", "level", "level", "Level_03"));

    auto recordPath = TempRecordPath("typed_record.tas");
    WritePackedRecord(recordPath, MakeRecordFrames(16, 1000.0f / 132.0f, 1000.0f / 132.0f));
    TASProject record(recordPath.string());

    const auto state = BuildMenuStatePresentation(TASMenuRuntimeSnapshot{});
    const auto scriptPresentation = BuildProjectPresentation(script, state);
    const auto recordPresentation = BuildProjectPresentation(record, state);

    EXPECT_EQ(scriptPresentation.rowBadgeLabel, "L");
    EXPECT_EQ(recordPresentation.rowBadgeLabel, "L");
    EXPECT_EQ(scriptPresentation.typeLabel, "SCRIPT");
    EXPECT_EQ(recordPresentation.typeLabel, "RECORD");
    EXPECT_TRUE(scriptPresentation.typeMarkerLabel.empty());
    EXPECT_TRUE(recordPresentation.typeMarkerLabel.empty());
}

TEST(TASMenuPresentationTest, UsesRecordSpecificPendingPlaybackCopy) {
    TASMenuRuntimeSnapshot snapshot;
    snapshot.pendingPlayback = true;
    snapshot.activeProjectName = "SR02_1.33.070";
    snapshot.activeProjectIsRecord = true;

    auto state = BuildMenuStatePresentation(snapshot);

    EXPECT_EQ(state.label, "Pending Playback");
    EXPECT_THAT(state.detail, HasSubstr("level load"));
    EXPECT_THAT(state.detail, Not(HasSubstr("script context")));
}

TEST(TASMenuPresentationTest, ShowsPlaybackProgressForRecordAndScript) {
    TASMenuRuntimeSnapshot recordSnapshot;
    recordSnapshot.runningPlayback = true;
    recordSnapshot.activeProjectName = "SR02_1.33.070";
    recordSnapshot.activeProjectIsRecord = true;
    recordSnapshot.currentTick = 33;
    recordSnapshot.frameCount = 132;
    auto recordState = BuildMenuStatePresentation(recordSnapshot);

    EXPECT_THAT(recordState.detail, HasSubstr("SR02_1.33.070"));
    EXPECT_THAT(recordState.detail, HasSubstr("33/132"));
    EXPECT_THAT(recordState.detail, HasSubstr("25%"));

    TASMenuRuntimeSnapshot scriptSnapshot;
    scriptSnapshot.runningPlayback = true;
    scriptSnapshot.activeProjectName = "LuaRuntimeSmoke";
    scriptSnapshot.activeProjectIsRecord = false;
    scriptSnapshot.currentTick = 42;
    auto scriptState = BuildMenuStatePresentation(scriptSnapshot);

    EXPECT_THAT(scriptState.detail, HasSubstr("LuaRuntimeSmoke"));
    EXPECT_THAT(scriptState.detail, HasSubstr("tick 42"));
}

TEST(TASMenuPresentationTest, PlaybackCompletedReturnsToIdleWithVisibleDetail) {
    TASMenuRuntimeSnapshot snapshot;
    snapshot.playbackCompleted = true;
    snapshot.activeProjectName = "SR02_1.33.070";

    auto state = BuildMenuStatePresentation(snapshot);

    EXPECT_EQ(state.label, "Idle");
    EXPECT_THAT(state.detail, HasSubstr("Playback completed"));
    EXPECT_THAT(state.detail, HasSubstr("SR02_1.33.070"));
    EXPECT_FALSE(state.stopAction.enabled);
    EXPECT_TRUE(state.refreshAction.enabled);
}

TEST(TASMenuPresentationTest, SummarizesGlobalActivityAndDisablesConflictingActions) {
    TASMenuRuntimeSnapshot snapshot;
    snapshot.pendingPlayback = true;
    snapshot.activeProjectName = "LevelScript";
    snapshot.activityDetail = "Waiting for Level_03";
    auto state = BuildMenuStatePresentation(snapshot);

    EXPECT_EQ(state.label, "Pending Playback");
    EXPECT_THAT(state.detail, HasSubstr("Waiting for Level_03"));
    EXPECT_TRUE(state.stopAction.enabled);
    EXPECT_EQ(state.stopAction.label, "Stop Playback");

    TASProject project("C:/TAS/LevelScript",
                       MakeManifest("LevelScript", "level", "level", "Level_03"));
    auto presentation = BuildProjectPresentation(project, state);
    EXPECT_FALSE(presentation.playAction.enabled);
    EXPECT_THAT(presentation.playAction.disabledReason, HasSubstr("Stop current TAS first"));
    EXPECT_EQ(presentation.rowStatusLabel, "WAIT");
    EXPECT_EQ(presentation.rowBadgeLabel, "...");
}

TEST(TASMenuPresentationTest, UsesSpecificStopLabelsForUserVisibleActivity) {
    TASMenuRuntimeSnapshot runningPlayback;
    runningPlayback.runningPlayback = true;
    runningPlayback.activeProjectName = "GlobalStartup";
    auto playbackState = BuildMenuStatePresentation(runningPlayback);
    EXPECT_EQ(playbackState.label, "Running Playback");
    EXPECT_EQ(playbackState.stopAction.label, "Stop Playback");
    EXPECT_TRUE(playbackState.stopAction.enabled);

    TASMenuRuntimeSnapshot recording;
    recording.recording = true;
    recording.frameCount = 42;
    auto recordingState = BuildMenuStatePresentation(recording);
    EXPECT_EQ(recordingState.label, "Recording");
    EXPECT_THAT(recordingState.detail, HasSubstr("42"));
    EXPECT_EQ(recordingState.stopAction.label, "Stop Recording");

    TASMenuRuntimeSnapshot translating;
    translating.translating = true;
    translating.activeProjectName = "SR02_1.33.070";
    auto translatingState = BuildMenuStatePresentation(translating);
    EXPECT_EQ(translatingState.label, "Translating");
    EXPECT_EQ(translatingState.stopAction.label, "Stop Translation");
}

TEST(TASMenuPresentationTest, ShowsGlobalStartupProjectSemantics) {
    TASProject project("C:/TAS/GlobalStartup",
                       MakeManifest("GlobalStartup", "global", "startup", ""));

    auto presentation = BuildProjectPresentation(project, BuildMenuStatePresentation(TASMenuRuntimeSnapshot{}));

    EXPECT_EQ(presentation.typeLabel, "SCRIPT");
    EXPECT_EQ(presentation.scopeLabel, "GLOBAL");
    EXPECT_EQ(presentation.triggerLabel, "startup");
    EXPECT_EQ(presentation.levelLabel, "Any");
    EXPECT_EQ(presentation.rowStatusLabel, "GLOBAL");
    EXPECT_EQ(presentation.rowBadgeLabel, "G");
    EXPECT_TRUE(presentation.playAction.enabled);
}

TEST(TASMenuPresentationTest, IgnoresLegacyScriptPreloadRngStateFromManifest) {
    TASProject project("C:/TAS/RngScript", MakeManifestWithLegacyPreloadRng());

    EXPECT_TRUE(project.IsValid());
}

TEST(TASMenuPresentationTest, SurfacesLastErrorWithoutHidingRecoveryActions) {
    TASMenuRuntimeSnapshot snapshot;
    snapshot.lastError = "Project directory cannot be read";

    auto state = BuildMenuStatePresentation(snapshot);

    EXPECT_EQ(state.label, "Error");
    EXPECT_EQ(state.failureReason, "Project directory cannot be read");
    EXPECT_FALSE(state.stopAction.enabled);
    EXPECT_EQ(state.refreshAction.label, "Refresh");
    EXPECT_TRUE(state.refreshAction.enabled);
}
