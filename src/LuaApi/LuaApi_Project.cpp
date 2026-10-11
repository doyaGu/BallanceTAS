#include "LuaApi.h"

#include "LuaApiCommon.h"
#include "LuaRuntime/LuaFunction.h"
#include "LuaRuntime/LuaStackGuard.h"

#include "GameInterface.h"
#include "Logger.h"
#include "ProjectManager.h"
#include "RecordPlayer.h"
#include "ScriptContext.h"
#include "ScriptContextManager.h"
#ifdef Yield
#undef Yield
#endif
#include "LuaScheduler.h"
#include "TASEngine.h"
#include "TASProject.h"
#include "TranslationService.h"

#include <filesystem>
#include <string>

using namespace LuaApiCommon;

static ProjectManager *RequireManager(lua_State *L, const char *where) {
    ScriptContext *context = GetContext(L);
    ProjectManager *manager = context ? context->GetProjectManager() : nullptr;
    if (!manager) {
        Raise(L, where, "the project manager is not available");
    }
    return manager;
}

static void PushStringField(lua_State *L, const char *name, const std::string &value) {
    PushString(L, value);
    lua_setfield(L, -2, name);
}

static void PushProjectInfo(lua_State *L, const TASProject &project) {
    lua_createtable(L, 0, 6);
    PushStringField(L, "name", project.GetName());
    PushStringField(L, "type", project.IsScriptProject() ? "script" : "record");
    PushStringField(L, "scope", project.IsGlobalProject() ? "global" : "level");
    PushStringField(L, "author", project.GetAuthor());
    PushStringField(L, "description", project.GetDescription());
    PushStringField(L, "target_level", project.GetTargetLevel());
}

static TASProject *FindProject(ProjectManager &manager, const char *name) {
    TASProject *project = manager.FindProject(name);
    return project && project->IsValid() ? project : nullptr;
}

// Stopping the context that is running this call would tear down the coroutine we are in.
static bool IsCallersProject(ScriptContext *context, const TASProject *project) {
    return project && context->GetCurrentProject() == project;
}

// Starts a project right away: scripts in their context, records in the record player.
static bool StartProjectNow(ScriptContext *context, TASProject *project, std::string &reason) {
    if (project->IsScriptProject()) {
        ScriptContextManager *contexts = context->GetScriptContextManager();
        if (!contexts) {
            reason = "the script context manager is not available";
            return false;
        }
        GameInterface *game = context->GetGameInterface();
        auto target = project->IsGlobalProject()
            ? contexts->GetOrCreateGlobalContext()
            : contexts->GetOrCreateLevelContext(ScriptContextManager::ResolveLevelKey(
                  project->GetTargetLevel(), game ? game->GetMapName() : "", game ? game->GetCurrentLevel() : 0));
        if (!target) {
            reason = "could not create a script context";
            return false;
        }
        if (!target->LoadAndExecute(project)) {
            reason = "the script failed to load (see the log)";
            return false;
        }
        return true;
    }
    RecordPlayer *player = context->GetRecordPlayer();
    if (!player) {
        reason = "the record player is not available";
        return false;
    }
    if (!player->LoadAndPlay(project->GetRecordFilePath())) {
        reason = "the record could not be loaded (see the log)";
        return false;
    }
    return true;
}

static void StopProject(ScriptContext *context, TASProject *project) {
    if (project->IsScriptProject()) {
        if (ScriptContextManager *contexts = context->GetScriptContextManager()) {
            for (const auto &scriptContext : contexts->GetContextsByPriority()) {
                if (scriptContext && scriptContext->GetCurrentProject() == project) {
                    scriptContext->Stop();
                }
            }
        }
    } else if (RecordPlayer *player = context->GetRecordPlayer()) {
        player->Stop();
    }
}

static int List(lua_State *L) {
    ProjectManager *manager = RequireManager(L, "tas.project.list");
    lua_newtable(L);
    lua_Integer index = 1;
    for (const auto &project : manager->GetProjects()) {
        if (project && project->IsValid()) {
            PushProjectInfo(L, *project);
            lua_rawseti(L, -2, index++);
        }
    }
    return 1;
}

// tas.project.get_current() -> info | nil
static int GetCurrent(lua_State *L) {
    TASProject *project = RequireManager(L, "tas.project.get_current")->GetCurrentProject();
    if (!project) {
        lua_pushnil(L);
        return 1;
    }
    PushProjectInfo(L, *project);
    return 1;
}

// tas.project.find(name) -> info | nil
static int Find(lua_State *L) {
    const char *name = CheckNonEmptyString(L, 1, "tas.project.find");
    TASProject *project = FindProject(*RequireManager(L, "tas.project.find"), name);
    if (!project) {
        lua_pushnil(L);
        return 1;
    }
    PushProjectInfo(L, *project);
    return 1;
}

static int IsLoaded(lua_State *L) {
    lua_pushboolean(L, RequireManager(L, "tas.project.is_loaded")->GetCurrentProject() != nullptr);
    return 1;
}

// tas.project.load(name) -> true | false, reason. Starts the project right away.
static int Load(lua_State *L) {
    const char *name = CheckNonEmptyString(L, 1, "tas.project.load");
    ProjectManager *manager = RequireManager(L, "tas.project.load");
    TASProject *project = FindProject(*manager, name);
    if (!project) {
        return PushFail(L, "no project with that name");
    }
    if (IsCallersProject(GetContext(L), project)) {
        return Raise(L, "tas.project.load", "a script cannot reload its own project");
    }
    manager->SetCurrentProject(project);
    Log::Info("Loading project: %s", project->GetName().c_str());
    std::string reason;
    const bool ok = StartProjectNow(GetContext(L), project, reason);
    return PushResult(L, ok, reason);
}

// tas.project.unload() -> true | false, reason
static int Unload(lua_State *L) {
    ProjectManager *manager = RequireManager(L, "tas.project.unload");
    TASProject *current = manager->GetCurrentProject();
    if (!current) {
        return PushFail(L, "no project is loaded");
    }
    if (IsCallersProject(GetContext(L), current)) {
        return Raise(L, "tas.project.unload", "a script cannot unload its own project");
    }
    Log::Info("Unloading project: %s", current->GetName().c_str());
    StopProject(GetContext(L), current);
    manager->SetCurrentProject(nullptr);
    return PushOk(L);
}

// tas.project.reload() -> true | false, reason
static int Reload(lua_State *L) {
    ProjectManager *manager = RequireManager(L, "tas.project.reload");
    TASProject *current = manager->GetCurrentProject();
    if (!current) {
        return PushFail(L, "no project is loaded");
    }
    if (IsCallersProject(GetContext(L), current)) {
        return Raise(L, "tas.project.reload", "a script cannot reload its own project");
    }
    Log::Info("Reloading project: %s", current->GetName().c_str());
    StopProject(GetContext(L), current);
    std::string reason;
    const bool ok = StartProjectNow(GetContext(L), current, reason);
    return PushResult(L, ok, reason);
}

static int PlaybackArmedPredicate(lua_State *L) {
    auto *engine = static_cast<TASEngine *>(lua_touserdata(L, lua_upvalueindex(1)));
    lua_pushboolean(L, !engine || engine->IsPendingPlay() || engine->IsPlaying());
    return 1;
}

static int PlayContinuation(lua_State *L, int, lua_KContext) {
    return PushOk(L);
}

struct PlayOptions {
    bool validate = false;
    const char *reference = nullptr; // owned by the options table
    bool exact = false;
};

static bool OptBoolField(lua_State *L, int table, const char *field, const char *where) {
    lua_getfield(L, table, field);
    if (!lua_isnil(L, -1) && !lua_isboolean(L, -1)) {
        Raise(L, where, "options.%s must be a boolean", field);
    }
    const bool value = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return value;
}

// play's options: { validate = bool, reference = dump path, exact = bool }. A reference or
// exact implies validate.
static PlayOptions CheckPlayOptions(lua_State *L, int arg, const char *where) {
    PlayOptions options;
    if (lua_isnoneornil(L, arg)) {
        return options;
    }
    luaL_checktype(L, arg, LUA_TTABLE);
    options.validate = OptBoolField(L, arg, "validate", where);
    options.exact = OptBoolField(L, arg, "exact", where);
    lua_getfield(L, arg, "reference");
    if (!lua_isnil(L, -1)) {
        if (lua_type(L, -1) != LUA_TSTRING || lua_rawlen(L, -1) == 0) {
            Raise(L, where, "options.reference must be a non-empty path");
        }
        options.reference = lua_tostring(L, -1);
    }
    lua_pop(L, 1);
    options.validate = options.validate || options.exact || options.reference;
    return options;
}

// tas.project.play(name[, options]) -> true | false, reason
// Arms a project the way the TAS menu's Play button does: playback starts when its level
// starts. Waits until the engine is armed, so a following tas.level.load is safe. With
// options.validate the playback writes a validation dump from its first frame, compared
// against options.reference when given, bit for bit when options.exact.
static int Play(lua_State *L) {
    const char *name = CheckNonEmptyString(L, 1, "tas.project.play");
    const PlayOptions options = CheckPlayOptions(L, 2, "tas.project.play");
    LuaScheduler *scheduler = RequireYieldableScheduler(L, "tas.project.play");
    ProjectManager *manager = RequireManager(L, "tas.project.play");
    ScriptContext *context = GetContext(L);
    TASEngine *engine = context->GetEngine();
    TASProject *project = FindProject(*manager, name);
    if (!project) {
        return PushFail(L, "no project with that name");
    }
    if (IsCallersProject(context, project)) {
        return Raise(L, "tas.project.play", "a script cannot replay its own project");
    }
    manager->SetCurrentProject(project);
    bool started = false;
    {
        // Gone before the yield below, which does not unwind C++ objects.
        TASEngine::ReplayValidation validation;
        validation.enabled = options.validate;
        validation.reference = options.reference ? options.reference : "";
        validation.exact = options.exact;
        started = engine->StartReplay(validation);
    }
    if (!started) {
        return PushFail(L, engine->IsIdle() ? "the project cannot be played" : "a TAS is already running or armed");
    }
    lua_pushlightuserdata(L, engine);
    lua_pushcclosure(L, PlaybackArmedPredicate, 1);
    scheduler->YieldUntil(tas::lua::LuaFunction::FromStack(L, -1));
    return lua_yieldk(L, 0, 0, PlayContinuation);
}

static int TranslationArmedPredicate(lua_State *L) {
    auto *engine = static_cast<TASEngine *>(lua_touserdata(L, lua_upvalueindex(1)));
    lua_pushboolean(L, !engine || engine->IsPendingTranslate() || engine->IsTranslating());
    return 1;
}

// tas.project.translate(name) -> true | false, reason
// Arms a translation like the TAS menu's Translate button: a record becomes a script, a
// level script becomes a record, while its level plays. Waits until armed; the outcome is
// tas.project.get_translation_result() once the level has played through.
static int Translate(lua_State *L) {
    const char *name = CheckNonEmptyString(L, 1, "tas.project.translate");
    LuaScheduler *scheduler = RequireYieldableScheduler(L, "tas.project.translate");
    ProjectManager *manager = RequireManager(L, "tas.project.translate");
    ScriptContext *context = GetContext(L);
    TASEngine *engine = context->GetEngine();
    TASProject *project = FindProject(*manager, name);
    if (!project) {
        return PushFail(L, "no project with that name");
    }
    if (IsCallersProject(context, project)) {
        return Raise(L, "tas.project.translate", "a script cannot translate its own project");
    }
    if (project->IsRecordProject() ? !project->CanBeTranslated() : !project->CanTranslateToRecord()) {
        return PushFail(L, project->IsRecordProject() ? project->GetTranslationCompatibilityMessage()
                                                      : project->GetScriptToRecordCompatibilityMessage());
    }
    manager->SetCurrentProject(project);
    if (!engine->StartTranslation()) {
        return PushFail(L, engine->IsIdle() ? "the project cannot be translated" : "a TAS is already running or armed");
    }
    lua_pushlightuserdata(L, engine);
    lua_pushcclosure(L, TranslationArmedPredicate, 1);
    scheduler->YieldUntil(tas::lua::LuaFunction::FromStack(L, -1));
    return lua_yieldk(L, 0, 0, PlayContinuation);
}

// tas.project.get_translation_result()
//   -> { ok, project = name | nil, path = string | nil, message } | nil (none finished yet)
// The output project is listed by the time this returns it.
static int GetTranslationResult(lua_State *L) {
    ScriptContext *context = GetContext(L);
    TASEngine *engine = context ? context->GetEngine() : nullptr;
    auto *translation = engine ? engine->GetServiceProvider().Resolve<TranslationService>() : nullptr;
    if (!translation || translation->GetResultSerial() == 0) {
        lua_pushnil(L);
        return 1;
    }
    if (ProjectManager *manager = context->GetProjectManager()) {
        manager->RefreshIfChanged();
    }

    lua_createtable(L, 0, 4);
    lua_pushboolean(L, translation->WasLastSuccessful());
    lua_setfield(L, -2, "ok");
    const std::string &path = translation->GetLastOutputPath();
    if (!path.empty()) {
        const std::filesystem::path output(path);
        // A record is named after its file, a script project after its directory.
        const std::string projectName = output.extension() == ".tas" ? output.stem().string()
                                                                     : output.filename().string();
        lua_pushlstring(L, projectName.data(), projectName.size());
        lua_setfield(L, -2, "project");
        lua_pushlstring(L, path.data(), path.size());
        lua_setfield(L, -2, "path");
    }
    const std::string &message = translation->GetLastResultMessage();
    lua_pushlstring(L, message.data(), message.size());
    lua_setfield(L, -2, "message");
    return 1;
}

// tas.project.stop_playback(): stops or disarms the playback started by play()
static int StopPlayback(lua_State *L) {
    ScriptContext *context = GetContext(L);
    if (TASEngine *engine = context ? context->GetEngine() : nullptr) {
        engine->StopReplay();
    }
    return 0;
}

// tas.project.get_playback_state() -> "idle" | "armed" | "playing" | "translating"
// "armed": a playback or translation waits for its level to load; "playing" includes a
// paused playback.
static int GetPlaybackState(lua_State *L) {
    ScriptContext *context = GetContext(L);
    TASEngine *engine = context ? context->GetEngine() : nullptr;
    const char *state = "idle";
    if (!engine) {
        lua_pushstring(L, state);
        return 1;
    }
    if (engine->IsPendingPlay() || engine->IsPendingTranslate()) {
        state = "armed";
    } else if (engine->IsTranslating()) {
        state = "translating";
    } else if (engine->IsPlaying() || (engine->IsPaused() && engine->GetPlaybackType() != PlaybackType::None)) {
        state = "playing";
    }
    lua_pushstring(L, state);
    return 1;
}

void LuaApi::RegisterProjectApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);
    lua_getglobal(state, "tas");
    lua_newtable(state);
    SetClosure(state, "list", List, context);
    SetClosure(state, "get_current", GetCurrent, context);
    SetClosure(state, "find", Find, context);
    SetClosure(state, "load", Load, context);
    SetClosure(state, "unload", Unload, context);
    SetClosure(state, "reload", Reload, context);
    SetClosure(state, "is_loaded", IsLoaded, context);
    SetClosure(state, "play", Play, context);
    SetClosure(state, "stop_playback", StopPlayback, context);
    SetClosure(state, "get_playback_state", GetPlaybackState, context);
    SetClosure(state, "translate", Translate, context);
    SetClosure(state, "get_translation_result", GetTranslationResult, context);
    lua_setfield(state, -2, "project");
    lua_pop(state, 1);
}
