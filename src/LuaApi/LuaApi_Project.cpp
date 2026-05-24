#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"

#include "GameInterface.h"
#include "Logger.h"
#include "ProjectManager.h"
#include "RecordPlayer.h"
#include "ScriptContext.h"
#include "ScriptContextManager.h"
#include "TASProject.h"

#include <stdexcept>

namespace {

ScriptContext *GetContext(lua_State *L) {
    return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

void SetProjectFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, function, 1);
    lua_setfield(L, -2, name);
}

void PushStringField(lua_State *L, const char *name, const std::string &value) {
    lua_pushlstring(L, value.data(), value.size());
    lua_setfield(L, -2, name);
}

void PushProjectInfo(lua_State *L, const TASProject &project) {
    lua_createtable(L, 0, 6);
    PushStringField(L, "name", project.GetName());
    PushStringField(L, "type", project.IsScriptProject() ? "script" : "record");
    PushStringField(L, "scope", project.IsGlobalProject() ? "global" : "level");
    PushStringField(L, "author", project.GetAuthor());
    PushStringField(L, "description", project.GetDescription());
    PushStringField(L, "target_level", project.GetTargetLevel());
}

const char *CheckProjectName(lua_State *L, int index, const char *functionName) {
    size_t length = 0;
    const char *name = luaL_checklstring(L, index, &length);
    if (length == 0) {
        luaL_error(L, "%s: project name cannot be empty", functionName);
    }
    return name;
}

TASProject *FindProject(ProjectManager &manager, const std::string &name) {
    const auto &projects = manager.GetProjects();
    for (const auto &project : projects) {
        if (project && project->IsValid() && project->GetName() == name) {
            return project.get();
        }
    }
    return nullptr;
}

std::string ResolveProjectLevelKey(const TASProject &project, GameInterface *game) {
    return ScriptContextManager::ResolveLevelKey(
        project.GetTargetLevel(),
        game ? game->GetMapName() : "",
        game ? game->GetCurrentLevel() : 0);
}

int List(lua_State *L) {
    auto *context = GetContext(L);
    auto *manager = context ? context->GetProjectManager() : nullptr;
    if (!manager) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    int index = 1;
    for (const auto &project : manager->GetProjects()) {
        if (project && project->IsValid()) {
            PushProjectInfo(L, *project);
            lua_seti(L, -2, index++);
        }
    }
    return 1;
}

int GetCurrent(lua_State *L) {
    auto *context = GetContext(L);
    auto *manager = context ? context->GetProjectManager() : nullptr;
    if (!manager || !manager->GetCurrentProject()) {
        lua_pushnil(L);
        return 1;
    }

    PushProjectInfo(L, *manager->GetCurrentProject());
    return 1;
}

int Find(lua_State *L) {
    auto *context = GetContext(L);
    const std::string name = CheckProjectName(L, 1, "project.find");
    auto *manager = context ? context->GetProjectManager() : nullptr;
    if (!manager) {
        lua_pushnil(L);
        return 1;
    }

    TASProject *project = FindProject(*manager, name);
    if (!project) {
        lua_pushnil(L);
        return 1;
    }

    PushProjectInfo(L, *project);
    return 1;
}

int Load(lua_State *L) {
    auto *context = GetContext(L);
    const std::string name = CheckProjectName(L, 1, "project.load");
    auto *manager = context ? context->GetProjectManager() : nullptr;
    if (!manager) {
        return luaL_error(L, "project.load: ProjectManager not available");
    }

    TASProject *project = FindProject(*manager, name);
    if (!project) {
        Log::Warn("project.load: Project '%s' not found", name.c_str());
        lua_pushboolean(L, 0);
        return 1;
    }

    manager->SetCurrentProject(project);
    if (project->IsScriptProject()) {
        auto *contextManager = context->GetScriptContextManager();
        auto *game = context->GetGameInterface();
        if (!contextManager) {
            return luaL_error(L, "project.load: ScriptContextManager not available");
        }

        const bool isGlobal = project->IsGlobalProject();
        auto targetContext = isGlobal
                                 ? contextManager->GetOrCreateGlobalContext()
                                 : contextManager->GetOrCreateLevelContext(ResolveProjectLevelKey(*project, game));
        if (!targetContext) {
            return luaL_error(L, "project.load: Failed to create script context");
        }

        Log::Info("Loading script project: %s", name.c_str());
        lua_pushboolean(L, targetContext->LoadAndExecute(project));
        return 1;
    }

    if (project->IsRecordProject()) {
        auto *recordPlayer = context->GetRecordPlayer();
        if (!recordPlayer) {
            return luaL_error(L, "project.load: RecordPlayer not available");
        }

        const std::string recordPath = project->GetRecordFilePath();
        Log::Info("Loading record project: %s (%s)", name.c_str(), recordPath.c_str());
        lua_pushboolean(L, recordPlayer->LoadAndPlay(recordPath));
        return 1;
    }

    lua_pushboolean(L, 0);
    return 1;
}

int Unload(lua_State *L) {
    auto *context = GetContext(L);
    auto *manager = context ? context->GetProjectManager() : nullptr;
    if (!manager) {
        return luaL_error(L, "project.unload: ProjectManager not available");
    }

    TASProject *current = manager->GetCurrentProject();
    if (!current) {
        Log::Warn("project.unload: No project currently loaded");
        return 0;
    }

    Log::Info("Unloading project: %s", current->GetName().c_str());
    if (current->IsScriptProject()) {
        auto *contextManager = context->GetScriptContextManager();
        if (contextManager) {
            for (const auto &scriptContext : contextManager->GetContextsByPriority()) {
                if (scriptContext && scriptContext->GetCurrentProject() == current) {
                    scriptContext->Stop();
                    Log::Info("Stopped context: %s", scriptContext->GetName().c_str());
                }
            }
        }
    } else if (current->IsRecordProject()) {
        auto *recordPlayer = context->GetRecordPlayer();
        if (recordPlayer) {
            recordPlayer->Stop();
        }
    }

    manager->SetCurrentProject(nullptr);
    return 0;
}

int Reload(lua_State *L) {
    auto *context = GetContext(L);
    auto *manager = context ? context->GetProjectManager() : nullptr;
    if (!manager) {
        return luaL_error(L, "project.reload: ProjectManager not available");
    }

    TASProject *current = manager->GetCurrentProject();
    if (!current) {
        Log::Warn("project.reload: No project currently loaded");
        lua_pushboolean(L, 0);
        return 1;
    }

    const std::string projectName = current->GetName();
    Log::Info("Reloading project: %s", projectName.c_str());

    if (current->IsScriptProject()) {
        auto *contextManager = context->GetScriptContextManager();
        if (contextManager) {
            for (const auto &scriptContext : contextManager->GetContextsByPriority()) {
                if (scriptContext && scriptContext->GetCurrentProject() == current) {
                    scriptContext->Stop();
                }
            }
        }
        if (!contextManager) {
            return luaL_error(L, "project.reload: ScriptContextManager not available");
        }

        auto *game = context->GetGameInterface();
        auto targetContext = current->IsGlobalProject()
                                 ? contextManager->GetOrCreateGlobalContext()
                                 : contextManager->GetOrCreateLevelContext(ResolveProjectLevelKey(*current, game));
        if (!targetContext) {
            return luaL_error(L, "project.reload: Failed to create script context");
        }

        lua_pushboolean(L, targetContext->LoadAndExecute(current));
        return 1;
    }

    if (current->IsRecordProject()) {
        auto *recordPlayer = context->GetRecordPlayer();
        lua_pushboolean(L, recordPlayer && recordPlayer->LoadAndPlay(current->GetRecordFilePath()));
        return 1;
    }

    lua_pushboolean(L, 0);
    return 1;
}

int IsLoaded(lua_State *L) {
    auto *context = GetContext(L);
    auto *manager = context ? context->GetProjectManager() : nullptr;
    lua_pushboolean(L, manager && manager->GetCurrentProject());
    return 1;
}

} // namespace

void LuaApi::RegisterProjectApi(lua_State *state, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterProjectApi requires a valid ScriptContext");
    }

    tas::lua::LuaStackGuard guard(state);
    lua_getglobal(state, "tas");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_setglobal(state, "tas");
        lua_getglobal(state, "tas");
    }

    lua_newtable(state);
    SetProjectFunction(state, "list", List, context);
    SetProjectFunction(state, "get_current", GetCurrent, context);
    SetProjectFunction(state, "find", Find, context);
    SetProjectFunction(state, "load", Load, context);
    SetProjectFunction(state, "unload", Unload, context);
    SetProjectFunction(state, "reload", Reload, context);
    SetProjectFunction(state, "is_loaded", IsLoaded, context);
    lua_setfield(state, -2, "project");

    lua_pop(state, 1);
}
