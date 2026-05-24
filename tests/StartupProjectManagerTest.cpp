#include "StartupProjectManager.h"

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "TASProject.h"

namespace {

tas::lua::LuaValue MakeValue(std::string value) {
    return tas::lua::LuaValue(tas::lua::LuaValue::Storage{std::move(value)});
}

tas::lua::LuaValue MakeValue(double value) {
    return tas::lua::LuaValue(tas::lua::LuaValue::Storage{static_cast<lua_Number>(value)});
}

tas::lua::LuaValue MakeManifest(std::string name,
                                std::string scope,
                                std::string trigger,
                                std::string level = "",
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

std::unique_ptr<TASProject> MakeProject(std::string name,
                                        std::string scope,
                                        std::string trigger,
                                        std::string level = "") {
    return std::make_unique<TASProject>(
        "C:/TAS/" + name,
        MakeManifest(std::move(name), std::move(scope), std::move(trigger), std::move(level)));
}

} // namespace

TEST(StartupProjectManagerTest, AutoSelectsFirstValidGlobalStartupProjectWhenNoProjectConfigured) {
    std::vector<std::unique_ptr<TASProject>> projects;
    projects.push_back(MakeProject("MenuGlobal", "global", "menu"));
    projects.push_back(MakeProject("LevelScript", "level", "level", "Level_01"));
    projects.push_back(MakeProject("BootGlobal", "global", "startup"));
    projects.push_back(MakeProject("SecondBootGlobal", "global", "startup"));

    TASProject *selected = StartupProjectManager::SelectProjectForContext(
        projects, "", "startup");

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->GetName(), "BootGlobal");
}

TEST(StartupProjectManagerTest, ConfiguredProjectMustStillBeEligibleForStartupContext) {
    std::vector<std::unique_ptr<TASProject>> projects;
    projects.push_back(MakeProject("MenuGlobal", "global", "menu"));
    projects.push_back(MakeProject("BootGlobal", "global", "startup"));

    TASProject *wrongTrigger = StartupProjectManager::SelectProjectForContext(
        projects, "MenuGlobal", "startup");
    EXPECT_EQ(wrongTrigger, nullptr);

    TASProject *configured = StartupProjectManager::SelectProjectForContext(
        projects, "BootGlobal", "startup");
    ASSERT_NE(configured, nullptr);
    EXPECT_EQ(configured->GetName(), "BootGlobal");
}

TEST(StartupProjectManagerTest, LevelContextComparesResolvedLevelKeys) {
    std::vector<std::unique_ptr<TASProject>> projects;
    projects.push_back(MakeProject("LevelGlobal", "global", "level", "Level_01"));

    TASProject *wrongLevel = StartupProjectManager::SelectProjectForContext(
        projects, "LevelGlobal", "level", "Level_02");
    EXPECT_EQ(wrongLevel, nullptr);

    TASProject *matchingLevel = StartupProjectManager::SelectProjectForContext(
        projects, "LevelGlobal", "level", "Level_01");
    ASSERT_NE(matchingLevel, nullptr);
    EXPECT_EQ(matchingLevel->GetName(), "LevelGlobal");
}
