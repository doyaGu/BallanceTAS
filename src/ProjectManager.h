#pragma once

#include <vector>
#include <memory>
#include <string>
#include <sol/sol.hpp>
#include "TASProject.h"

class BallanceTAS;

/**
 * @class ProjectManager
 * @brief Discovers, loads, and manages TAS projects from the filesystem.
 */
class ProjectManager {
public:
    ProjectManager(BallanceTAS *mod, sol::state &lua_state);

    /**
     * @brief Scans the TAS directory and reloads the list of all available projects.
     */
    void RefreshProjects();

    // --- Accessors ---
    const std::vector<std::unique_ptr<TASProject>> &GetProjects() const { return m_Projects; }
    TASProject *GetCurrentProject() const { return m_CurrentProject; }
    void SetCurrentProject(TASProject *project) { m_CurrentProject = project; }

private:
    /**
     * @brief Loads a single project's manifest and returns a TASProject object.
     * @param projectPath The root directory of the project to load.
     * @return A unique_ptr to the TASProject, or nullptr if loading fails.
     */
    std::unique_ptr<TASProject> LoadProject(const std::string &projectPath);

    /**
     * @brief Parses a manifest.lua file in a secure, sandboxed Lua environment.
     * @param path The full path to the manifest.lua file.
     * @return A sol::table containing the manifest data, or an invalid table on error.
     */
    sol::table ParseManifestFile(const std::string &path);

    BallanceTAS *m_Mod;
    sol::state &m_LuaState; // Reference to the main Lua state for creating sandboxes.
    std::string m_TASRootPath;

    std::vector<std::unique_ptr<TASProject>> m_Projects;
    TASProject *m_CurrentProject = nullptr; // Current project being worked on, if any.
};
