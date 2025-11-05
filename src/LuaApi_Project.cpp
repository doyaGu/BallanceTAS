#include "LuaApi.h"

#include "Logger.h"
#include <stdexcept>

#include "TASEngine.h"
#include "ProjectManager.h"
#include "TASProject.h"
#include "RecordPlayer.h"
#include "ScriptContextManager.h"
#include "ScriptContext.h"
#include "GameInterface.h"

// ===================================================================
// Project Management API Registration
// ===================================================================

void LuaApi::RegisterProjectApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterProjectApi requires a valid ScriptContext");
    }

    // Create nested 'project' table
    sol::table project = tas["project"] = tas.create();

    // tas.project.list() - List all available projects
    project["list"] = [context]() -> sol::object {
        auto *pm = context->GetProjectManager();
        if (!pm) {
            return sol::nil;
        }

        const auto &projects = pm->GetProjects();
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();

        int index = 1;
        for (const auto &proj : projects) {
            if (proj && proj->IsValid()) {
                sol::table projInfo = lua.create_table();
                projInfo["name"] = proj->GetName();
                projInfo["type"] = proj->IsScriptProject() ? "script" : "record";
                projInfo["scope"] = proj->IsGlobalProject() ? "global" : "level";
                projInfo["author"] = proj->GetAuthor();
                projInfo["description"] = proj->GetDescription();
                projInfo["target_level"] = proj->GetTargetLevel();
                result[index++] = projInfo;
            }
        }

        return result;
    };

    // tas.project.get_current() - Get current project info
    project["get_current"] = [context]() -> sol::object {
        auto *pm = context->GetProjectManager();
        if (!pm) {
            return sol::nil;
        }

        TASProject *currentProj = pm->GetCurrentProject();
        if (!currentProj) {
            return sol::nil;
        }

        auto &lua = context->GetLuaState();
        sol::table projInfo = lua.create_table();
        projInfo["name"] = currentProj->GetName();
        projInfo["type"] = currentProj->IsScriptProject() ? "script" : "record";
        projInfo["scope"] = currentProj->IsGlobalProject() ? "global" : "level";
        projInfo["author"] = currentProj->GetAuthor();
        projInfo["description"] = currentProj->GetDescription();
        projInfo["target_level"] = currentProj->GetTargetLevel();
        return projInfo;
    };

    // tas.project.find(name) - Find a project by name
    project["find"] = [context](const std::string &projectName) -> sol::object {
        if (projectName.empty()) {
            throw sol::error("project.find: project name cannot be empty");
        }

        auto *pm = context->GetProjectManager();
        if (!pm) {
            return sol::nil;
        }

        const auto &projects = pm->GetProjects();
        for (const auto &proj : projects) {
            if (proj && proj->IsValid() && proj->GetName() == projectName) {
                auto &lua = context->GetLuaState();
                sol::table projInfo = lua.create_table();
                projInfo["name"] = proj->GetName();
                projInfo["type"] = proj->IsScriptProject() ? "script" : "record";
                projInfo["scope"] = proj->IsGlobalProject() ? "global" : "level";
                projInfo["author"] = proj->GetAuthor();
                projInfo["description"] = proj->GetDescription();
                projInfo["target_level"] = proj->GetTargetLevel();
                return projInfo;
            }
        }

        return sol::nil;
    };

    // tas.project.load(name) - Load and execute a project (script or record)
    project["load"] = [context](const std::string &projectName) -> bool {
        if (projectName.empty()) {
            throw sol::error("project.load: project name cannot be empty");
        }

        auto *pm = context->GetProjectManager();
        if (!pm) {
            throw sol::error("project.load: ProjectManager not available");
        }

        // Find the project
        const auto &projects = pm->GetProjects();
        TASProject *targetProject = nullptr;
        for (const auto &proj : projects) {
            if (proj && proj->IsValid() && proj->GetName() == projectName) {
                targetProject = proj.get();
                break;
            }
        }

        if (!targetProject) {
            Log::Warn("project.load: Project '%s' not found", projectName.c_str());
            return false;
        }

        // Set as current project
        pm->SetCurrentProject(targetProject);

        // Load based on project type
        if (targetProject->IsScriptProject()) {
            // Get the script context manager
            auto *ctxMgr = context->GetScriptContextManager();
            if (!ctxMgr) {
                throw sol::error("project.load: ScriptContextManager not available");
            }

            // Determine context type
            bool isGlobal = targetProject->IsGlobalProject();
            auto ctx = isGlobal
                ? ctxMgr->GetOrCreateGlobalContext()
                : ctxMgr->GetOrCreateLevelContext(std::to_string(context->GetGameInterface()->GetCurrentLevel()));

            if (!ctx) {
                throw sol::error("project.load: Failed to create script context");
            }

            Log::Info("Loading script project: %s", projectName.c_str());
            return ctx->LoadAndExecute(targetProject);
        } else if (targetProject->IsRecordProject()) {
            // Load record project
            auto *recordPlayer = context->GetRecordPlayer();
            if (!recordPlayer) {
                throw sol::error("project.load: RecordPlayer not available");
            }

            std::string recordPath = targetProject->GetRecordFilePath();
            Log::Info("Loading record project: %s (%s)", projectName.c_str(), recordPath.c_str());
            return recordPlayer->LoadAndPlay(recordPath);
        }

        return false;
    };

    // tas.project.unload() - Stop current project
    project["unload"] = [context]() {
        auto *pm = context->GetProjectManager();
        if (!pm) {
            throw sol::error("project.unload: ProjectManager not available");
        }

        TASProject *currentProj = pm->GetCurrentProject();
        if (!currentProj) {
            Log::Warn("project.unload: No project currently loaded");
            return;
        }

        Log::Info("Unloading project: %s", currentProj->GetName().c_str());

        // Stop based on project type
        if (currentProj->IsScriptProject()) {
            auto *ctxMgr = context->GetScriptContextManager();
            if (ctxMgr) {
                // Find and stop all contexts executing this project
                auto contexts = ctxMgr->GetContextsByPriority();
                for (const auto &ctx : contexts) {
                    if (ctx && ctx->GetCurrentProject() == currentProj) {
                        ctx->Stop();
                        Log::Info("Stopped context: %s", ctx->GetName().c_str());
                    }
                }
            }
        } else if (currentProj->IsRecordProject()) {
            auto *recordPlayer = context->GetRecordPlayer();
            if (recordPlayer) {
                recordPlayer->Stop();
            }
        }

        pm->SetCurrentProject(nullptr);
    };

    // tas.project.reload() - Reload current project
    project["reload"] = [context]() -> bool {
        auto *pm = context->GetProjectManager();
        if (!pm) {
            throw sol::error("project.reload: ProjectManager not available");
        }

        TASProject *currentProj = pm->GetCurrentProject();
        if (!currentProj) {
            Log::Warn("project.reload: No project currently loaded");
            return false;
        }

        std::string projectName = currentProj->GetName();
        Log::Info("Reloading project: %s", projectName.c_str());

        // Unload first
        if (currentProj->IsScriptProject()) {
            auto *ctxMgr = context->GetScriptContextManager();
            if (ctxMgr) {
                // Stop all contexts executing this project
                auto contexts = ctxMgr->GetContextsByPriority();
                for (const auto &ctx : contexts) {
                    if (ctx && ctx->GetCurrentProject() == currentProj) {
                        ctx->Stop();
                    }
                }
            }
        } else if (currentProj->IsRecordProject()) {
            auto *recordPlayer = context->GetRecordPlayer();
            if (recordPlayer) {
                recordPlayer->Stop();
            }
        }

        // Reload
        if (currentProj->IsScriptProject()) {
            auto *ctxMgr = context->GetScriptContextManager();
            if (!ctxMgr) {
                throw sol::error("project.reload: ScriptContextManager not available");
            }

            // Determine context type and reload
            bool isGlobal = currentProj->IsGlobalProject();
            auto ctx = isGlobal
                ? ctxMgr->GetOrCreateGlobalContext()
                : ctxMgr->GetOrCreateLevelContext(std::to_string(context->GetGameInterface()->GetCurrentLevel()));

            if (!ctx) {
                throw sol::error("project.reload: Failed to create script context");
            }

            return ctx->LoadAndExecute(currentProj);
        } else if (currentProj->IsRecordProject()) {
            auto *recordPlayer = context->GetRecordPlayer();
            if (recordPlayer) {
                std::string recordPath = currentProj->GetRecordFilePath();
                return recordPlayer->LoadAndPlay(recordPath);
            }
        }

        return false;
    };

    // tas.project.is_loaded() - Check if any project is loaded
    project["is_loaded"] = [context]() -> bool {
        auto *pm = context->GetProjectManager();
        if (!pm) {
            return false;
        }
        return pm->GetCurrentProject() != nullptr;
    };
}
