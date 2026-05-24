#include "ScriptContextManager.h"

#include <gtest/gtest.h>

TEST(ScriptContextManagerTest, ResolvesStableLevelKeysByPriority) {
    EXPECT_EQ(ScriptContextManager::ResolveLevelKey("Level_01", "Map_A", 3), "Level_01");
    EXPECT_EQ(ScriptContextManager::ResolveLevelKey("", "Map_A", 3), "Map_A");
    EXPECT_EQ(ScriptContextManager::ResolveLevelKey("  ", "  Map_A  ", 3), "Map_A");
    EXPECT_EQ(ScriptContextManager::ResolveLevelKey("", "", 3), "Level_3");
}

TEST(ScriptContextManagerTest, UsesStableContextNames) {
    EXPECT_EQ(ScriptContextManager::GlobalContextName(), "global");
    EXPECT_EQ(ScriptContextManager::MakeLevelContextName("Level_01"), "level:Level_01");
    EXPECT_EQ(ScriptContextManager::MakeLevelContextName("  Map_A  "), "level:Map_A");
}
