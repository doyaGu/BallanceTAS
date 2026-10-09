#include "LevelNames.h"

#include <gtest/gtest.h>

TEST(LevelNamesTest, ParsesNumbersAndLevelKeys) {
    EXPECT_EQ(LevelNames::Parse("3"), 3);
    EXPECT_EQ(LevelNames::Parse("13"), 13);
    EXPECT_EQ(LevelNames::Parse("Level_03"), 3);
    EXPECT_EQ(LevelNames::Parse("Level 12"), 12);
    EXPECT_EQ(LevelNames::Parse("level1"), 1);
    EXPECT_EQ(LevelNames::Parse("LEVEL_07"), 7);
}

TEST(LevelNamesTest, RejectsNonBuiltinLevels) {
    EXPECT_FALSE(LevelNames::Parse("0").has_value());
    EXPECT_FALSE(LevelNames::Parse("14").has_value());
    EXPECT_FALSE(LevelNames::Parse("Level_100").has_value());
    EXPECT_FALSE(LevelNames::Parse("Level_").has_value());
    EXPECT_FALSE(LevelNames::Parse("Custom").has_value());
    EXPECT_FALSE(LevelNames::Parse("").has_value());
}

TEST(LevelNamesTest, FormatsLevelKeys) {
    EXPECT_EQ(LevelNames::ToKey(1), "Level_01");
    EXPECT_EQ(LevelNames::ToKey(13), "Level_13");
}
