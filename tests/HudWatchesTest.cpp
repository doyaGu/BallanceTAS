#include <gtest/gtest.h>

#include "Workflow/HudWatches.h"

namespace {

TEST(HudWatchesTest, KeepsFirstSetOrderAndUpdatesInPlace) {
    HudWatches watches;
    watches.Set("level", "speed", "1");
    watches.Set("global", "lap", "2");
    watches.Set("level", "speed", "3");

    const auto &entries = watches.Entries();
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].name, "speed");
    EXPECT_EQ(entries[0].value, "3");
    EXPECT_EQ(entries[1].name, "lap");
}

TEST(HudWatchesTest, SameNameFromTwoContextsAreSeparate) {
    HudWatches watches;
    watches.Set("level", "x", "1");
    watches.Set("global", "x", "2");
    watches.Clear("level", "x");

    ASSERT_EQ(watches.Entries().size(), 1u);
    EXPECT_EQ(watches.Entries()[0].owner, "global");
}

TEST(HudWatchesTest, ClearOwnerDropsOnlyThatContext) {
    HudWatches watches;
    watches.Set("level", "a", "1");
    watches.Set("global", "b", "2");
    watches.Set("level", "c", "3");
    watches.ClearOwner("level");

    ASSERT_EQ(watches.Entries().size(), 1u);
    EXPECT_EQ(watches.Entries()[0].name, "b");
    watches.Clear("global", "missing");
    EXPECT_EQ(watches.Entries().size(), 1u);
}

} // namespace
