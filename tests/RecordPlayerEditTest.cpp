#include "RecordPlayer.h"

#include "GameInterface.h"

#include <gtest/gtest.h>

// RecordPlayer only asks the game for key remapping when playback starts, which these tests never do.
CKKEYBOARD GameInterface::RemapKey(CKKEYBOARD key) const {
    return key;
}

namespace {
// Editing never touches the engine; only playback start does.
RecordPlayer MakePlayer() {
    return RecordPlayer(reinterpret_cast<TASEngine *>(0x1));
}

bool KeyUp(const RecordPlayer &player, size_t frame) {
    RecordFrameData data;
    EXPECT_TRUE(player.GetFrameInput(frame, &data));
    return data.keyState.key_up != 0;
}
}

TEST(RecordPlayerEditTest, UndoAndRedoRestoreFrames) {
    RecordPlayer player = MakePlayer();
    ASSERT_TRUE(player.InsertFrames(0, 10));
    ASSERT_TRUE(player.SetFrameKey(3, "up", true));
    ASSERT_TRUE(player.DeleteFrames(0, 2));
    EXPECT_EQ(player.GetTotalFrames(), 8u);
    EXPECT_TRUE(KeyUp(player, 1));

    ASSERT_TRUE(player.Undo());
    EXPECT_EQ(player.GetTotalFrames(), 10u);
    EXPECT_TRUE(KeyUp(player, 3));

    ASSERT_TRUE(player.Undo());
    EXPECT_FALSE(KeyUp(player, 3));

    ASSERT_TRUE(player.Redo());
    EXPECT_TRUE(KeyUp(player, 3));
    ASSERT_TRUE(player.Redo());
    EXPECT_EQ(player.GetTotalFrames(), 8u);
    EXPECT_FALSE(player.CanRedo());
}

TEST(RecordPlayerEditTest, FailedEditsLeaveNoHistory) {
    RecordPlayer player = MakePlayer();
    ASSERT_TRUE(player.InsertFrames(0, 4));
    const size_t history = player.GetHistorySize();
    EXPECT_FALSE(player.SetFrameKey(10, "up", true));
    EXPECT_FALSE(player.SetFrameKey(1, "not_a_key", true));
    EXPECT_FALSE(player.AddSection(3, 1, "bad"));
    EXPECT_EQ(player.GetHistorySize(), history);
}

TEST(RecordPlayerEditTest, UndoCoversAnnotations) {
    RecordPlayer player = MakePlayer();
    ASSERT_TRUE(player.InsertFrames(0, 20));
    ASSERT_TRUE(player.AddSection(0, 9, "intro"));
    ASSERT_TRUE(player.AddMarker(5, "jump"));
    ASSERT_TRUE(player.RenameMarker("jump", "big_jump"));

    ASSERT_TRUE(player.Undo());
    EXPECT_NE(player.GetMarker("jump"), nullptr);
    EXPECT_EQ(player.GetMarker("big_jump"), nullptr);
    ASSERT_TRUE(player.Undo());
    EXPECT_EQ(player.GetMarker("jump"), nullptr);
    ASSERT_TRUE(player.Undo());
    EXPECT_EQ(player.GetSection("intro"), nullptr);
}

TEST(RecordPlayerEditTest, BranchesKeepTheirOwnFrames) {
    RecordPlayer player = MakePlayer();
    ASSERT_TRUE(player.InsertFrames(0, 6));
    ASSERT_TRUE(player.CreateBranch("alt", 2));
    ASSERT_TRUE(player.SwitchBranch("alt"));
    EXPECT_EQ(player.GetCurrentBranch(), "alt");
    ASSERT_TRUE(player.SetFrameKey(4, "up", true));
    ASSERT_TRUE(player.InsertFrames(6, 3));
    EXPECT_FALSE(player.DeleteBranch("alt"));

    ASSERT_TRUE(player.SwitchBranch(""));
    EXPECT_EQ(player.GetTotalFrames(), 6u);
    EXPECT_FALSE(KeyUp(player, 4));

    ASSERT_TRUE(player.SwitchBranch("alt"));
    EXPECT_EQ(player.GetTotalFrames(), 9u);
    EXPECT_TRUE(KeyUp(player, 4));

    ASSERT_TRUE(player.SwitchBranch(""));
    EXPECT_TRUE(player.DeleteBranch("alt"));
    EXPECT_FALSE(player.SwitchBranch("alt"));
}

TEST(RecordPlayerEditTest, UndoCoversMacrosMetadataAndLinks) {
    RecordPlayer player = MakePlayer();
    ASSERT_TRUE(player.InsertFrames(0, 8));
    ASSERT_TRUE(player.SaveMacro("hop", 0, 3));
    player.SetMetadataField("author", "tester");
    player.AddTag("wip");
    ASSERT_TRUE(player.LinkSavestate(4, "state.tss"));

    ASSERT_TRUE(player.Undo());
    EXPECT_EQ(player.GetSavestateLink(4), nullptr);
    ASSERT_TRUE(player.Undo());
    EXPECT_TRUE(player.GetTags().empty());
    ASSERT_TRUE(player.Undo());
    EXPECT_EQ(player.GetMetadataField("author"), "");
    ASSERT_TRUE(player.Undo());
    EXPECT_EQ(player.GetMacro("hop"), nullptr);
}

TEST(RecordPlayerEditTest, AnnotationsRoundTripThroughSidecar) {
    const std::string path = ::testing::TempDir() + "record_annotations_test.json";
    {
        RecordPlayer player = MakePlayer();
        ASSERT_TRUE(player.InsertFrames(0, 12));
        ASSERT_TRUE(player.AddSection(0, 5, "intro", "start", "#ff0000"));
        ASSERT_TRUE(player.AddMarker(7, "jump", MarkerType::Trick, "big one"));
        player.AddRangeComment(2, 4, "careful", "me", "route");
        ASSERT_TRUE(player.SaveMacro("hop", 1, 2, "two frames"));
        player.SetMetadataField("title", "Level 1 any%");
        player.AddTag("wip");
        ASSERT_TRUE(player.LinkSavestate(6, "state.tss", "before jump"));
        ASSERT_TRUE(player.CreateBranch("alt", 3));
        ASSERT_TRUE(player.SwitchBranch("alt"));
        ASSERT_TRUE(player.SetFrameKey(9, "up", true));
        ASSERT_TRUE(player.SaveAnnotations(path));
    }

    RecordPlayer loaded = MakePlayer();
    ASSERT_TRUE(loaded.InsertFrames(0, 12)); // stands in for the .tas frames of the active branch
    loaded.ClearHistory();
    ASSERT_TRUE(loaded.LoadAnnotations(path));

    ASSERT_NE(loaded.GetSection("intro"), nullptr);
    EXPECT_EQ(loaded.GetSection("intro")->color, "#ff0000");
    ASSERT_NE(loaded.GetMarker("jump"), nullptr);
    EXPECT_EQ(loaded.GetMarker("jump")->type, MarkerType::Trick);
    ASSERT_EQ(loaded.GetAllComments().size(), 1u);
    EXPECT_EQ(loaded.GetAllComments()[0].endFrame, 4u);
    ASSERT_NE(loaded.GetMacro("hop"), nullptr);
    EXPECT_EQ(loaded.GetMacro("hop")->frames.size(), 2u);
    EXPECT_EQ(loaded.GetMetadataField("title"), "Level 1 any%");
    EXPECT_EQ(loaded.GetTags(), std::vector<std::string>{"wip"});
    ASSERT_NE(loaded.GetSavestateLink(6), nullptr);
    EXPECT_EQ(loaded.GetCurrentBranch(), "alt");

    ASSERT_TRUE(loaded.SwitchBranch(""));
    EXPECT_EQ(loaded.GetTotalFrames(), 12u);
    std::remove(path.c_str());
}

TEST(RecordPlayerEditTest, CommentIdsSurviveRemoval) {
    RecordPlayer player = MakePlayer();
    ASSERT_TRUE(player.InsertFrames(0, 4));
    const size_t first = player.AddComment(0, "first");
    const size_t second = player.AddComment(1, "second");
    ASSERT_TRUE(player.RemoveComment(first));
    ASSERT_NE(player.GetComment(second), nullptr);
    EXPECT_EQ(player.GetComment(second)->text, "second");
    EXPECT_EQ(player.GetComment(first), nullptr);
    EXPECT_FALSE(player.RemoveComment(first));
}

TEST(RecordPlayerEditTest, MarkerTypeFilter) {
    RecordPlayer player = MakePlayer();
    ASSERT_TRUE(player.InsertFrames(0, 10));
    ASSERT_TRUE(player.AddMarker(3, "trick", MarkerType::Trick));
    ASSERT_TRUE(player.AddMarker(6, "note", MarkerType::Bookmark));
    ASSERT_NE(player.GetNextMarker(0), nullptr);
    EXPECT_EQ(player.GetNextMarker(0)->name, "trick");
    ASSERT_NE(player.GetNextMarker(0, MarkerType::Bookmark), nullptr);
    EXPECT_EQ(player.GetNextMarker(0, MarkerType::Bookmark)->name, "note");
    EXPECT_EQ(player.GetNextMarker(6, MarkerType::Trick), nullptr);
}

TEST(RecordPlayerEditTest, FindFramesAcceptsInputSeparators) {
    RecordPlayer player = MakePlayer();
    ASSERT_TRUE(player.InsertFrames(0, 3));
    ASSERT_TRUE(player.SetFrameKey(1, "up", true));
    ASSERT_TRUE(player.SetFrameKey(1, "space", true));
    for (const char *keys : {"up space", "up,space", "up;space", "up+space", "UP  Space"}) {
        const auto frames = player.FindFramesWithKeys(keys);
        ASSERT_EQ(frames.size(), 1u) << keys;
        EXPECT_EQ(frames[0], 1u);
    }
}
