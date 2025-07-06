#include "ProjectManager.h"

#include <filesystem>

#include "BallanceTAS.h"

namespace fs = std::filesystem;

ProjectManager::ProjectManager(BallanceTAS *mod, sol::state &lua_state)
    : m_Mod(mod), m_LuaState(lua_state) {
    // Define the root directory for all TAS projects.
    m_TASRootPath = BML_TAS_PATH;

    // Ensure the directory exists.
    if (!fs::exists(m_TASRootPath)) {
        fs::create_directory(m_TASRootPath);
    }

    RefreshProjects();
}

void ProjectManager::RefreshProjects() {
    m_Projects.clear();
    m_Mod->GetLogger()->Info("Scanning for TAS projects in: %s", m_TASRootPath.c_str());

    try {
        for (const auto &entry : fs::directory_iterator(m_TASRootPath)) {
            if (entry.is_directory()) {
                std::string projectPath = entry.path().string();
                std::string manifestPath = projectPath + "/manifest.lua";
                if (fs::exists(manifestPath)) {
                    // We found a potential project, try to load its manifest.
                    auto project = LoadProject(projectPath);
                    if (project && project->IsValid()) {
                        m_Projects.push_back(std::move(project));
                    }
                }
            }
        }
    } catch (const fs::filesystem_error &e) {
        m_Mod->GetLogger()->Error("Filesystem error while scanning for projects: %s", e.what());
    }

    // Sort projects alphabetically by name for consistent UI display.
    std::sort(m_Projects.begin(), m_Projects.end(), [](const auto &a, const auto &b) {
        return a->GetName() < b->GetName();
    });

    m_Mod->GetLogger()->Info("Found %d valid TAS projects.", m_Projects.size());
}

std::unique_ptr<TASProject> ProjectManager::LoadProject(const std::string &projectPath) {
    std::string manifestPath = projectPath + "/manifest.lua";
    sol::table manifest_table = ParseManifestFile(manifestPath);

    if (!manifest_table.valid()) {
        m_Mod->GetLogger()->Warn("Could not parse manifest for project at: %s", projectPath.c_str());
        return nullptr;
    }

    auto project = std::make_unique<TASProject>(projectPath, manifest_table);

    return project;
}

sol::table ProjectManager::ParseManifestFile(const std::string &path) {
    try {
        sol::table envTable = m_LuaState.create_table();
        envTable[sol::metatable_key] = m_LuaState.create_table_with(
            sol::meta_method::index, m_LuaState.globals()
        );

        sol::environment env(m_LuaState, envTable);

        sol::protected_function_result result = m_LuaState.safe_script_file(path, env);

        if (!result.valid()) {
            sol::error err = result;
            m_Mod->GetLogger()->Error("Error parsing manifest file '%s': %s", path.c_str(), err.what());
            return sol::table();
        }

        if (result.get_type() == sol::type::table) {
            return result.get<sol::table>();
        }

        m_Mod->GetLogger()->Warn("Manifest file '%s' did not return a table.", path.c_str());
        return sol::table();

    } catch (const sol::error& e) {
        m_Mod->GetLogger()->Error("Exception while parsing manifest '%s': %s", path.c_str(), e.what());
        return sol::table();
    }
}
