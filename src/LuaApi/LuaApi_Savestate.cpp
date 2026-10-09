#include "LuaApi.h"

#include "LuaApiCommon.h"
#include "LuaApiValues.h"
#include "LuaRuntime/LuaStackGuard.h"

#ifdef Yield
#undef Yield
#endif
#include "LuaScheduler.h"
#include "SavestateManager.h"
#include "ScriptContext.h"

using namespace LuaApiCommon;

static SavestateManager *RequireManager(lua_State *L, const char *where) {
    ScriptContext *context = GetContext(L);
    SavestateManager *manager = context ? context->GetSavestateManager() : nullptr;
    if (!manager) {
        Raise(L, where, "the savestate manager is not available");
    }
    return manager;
}

static int PushVoidResult(lua_State *L, const Result<void> &result) {
    return result.IsOk() ? PushOk(L) : PushFail(L, result.GetError().message);
}

static void PushSavestateInfo(lua_State *L, const SavestateData &data) {
    lua_newtable(L);
    PushString(L, data.name);
    lua_setfield(L, -2, "name");
    PushString(L, data.timestamp);
    lua_setfield(L, -2, "timestamp");
    PushString(L, data.levelName);
    lua_setfield(L, -2, "level_name");
    lua_pushinteger(L, data.levelNumber);
    lua_setfield(L, -2, "level_number");
    PushString(L, data.description);
    lua_setfield(L, -2, "description");
    PushVxVector(L, data.position);
    lua_setfield(L, -2, "position");
    lua_pushinteger(L, data.points);
    lua_setfield(L, -2, "points");
    lua_pushinteger(L, data.lives);
    lua_setfield(L, -2, "lives");
    lua_pushinteger(L, data.sector);
    lua_setfield(L, -2, "sector");
    lua_pushnumber(L, data.srScore);
    lua_setfield(L, -2, "sr_score");
    lua_pushnumber(L, data.hsScore);
    lua_setfield(L, -2, "hs_score");
    lua_pushinteger(L, static_cast<lua_Integer>(data.tick));
    lua_setfield(L, -2, "tick");
}

// tas.savestate.save(name[, description]) -> true | false, reason
static int Save(lua_State *L) {
    const char *name = CheckNonEmptyString(L, 1, "tas.savestate.save");
    const char *description = luaL_optstring(L, 2, "");
    SavestateManager *manager = RequireManager(L, "tas.savestate.save");
    return PushVoidResult(L, manager->SaveState(name, description));
}

static int LoadContinuation(lua_State *L, int, lua_KContext) {
    return PushOk(L);
}

// tas.savestate.load(name) -> true | false, reason. Waits a few frames after loading.
static int Load(lua_State *L) {
    const char *name = CheckNonEmptyString(L, 1, "tas.savestate.load");
    LuaScheduler *scheduler = RequireYieldableScheduler(L, "tas.savestate.load");
    SavestateManager *manager = RequireManager(L, "tas.savestate.load");
    bool loaded = false;
    {
        auto result = manager->LoadState(name);
        loaded = result.IsOk();
        if (!loaded) {
            PushFail(L, result.GetError().message);
        }
    }
    if (!loaded) {
        return 2;
    }
    // Give the game a few frames to settle.
    scheduler->YieldTicks(5);
    return lua_yieldk(L, 0, 0, LoadContinuation);
}

// tas.savestate.remove(name) -> true | false, reason
static int Remove(lua_State *L) {
    const char *name = CheckNonEmptyString(L, 1, "tas.savestate.remove");
    SavestateManager *manager = RequireManager(L, "tas.savestate.remove");
    return PushVoidResult(L, manager->DeleteState(name));
}

// tas.savestate.list() -> names | nil, reason
static int List(lua_State *L) {
    auto result = RequireManager(L, "tas.savestate.list")->ListStates();
    if (result.IsError()) {
        return PushNilReason(L, result.GetError().message);
    }
    PushStringList(L, result.Unwrap());
    return 1;
}

static int Exists(lua_State *L) {
    const char *name = CheckNonEmptyString(L, 1, "tas.savestate.exists");
    lua_pushboolean(L, RequireManager(L, "tas.savestate.exists")->StateExists(name));
    return 1;
}

// tas.savestate.get_info(name) -> info table | nil (no such savestate)
static int GetInfo(lua_State *L) {
    const char *name = CheckNonEmptyString(L, 1, "tas.savestate.get_info");
    auto result = RequireManager(L, "tas.savestate.get_info")->GetStateInfo(name);
    if (result.IsError()) {
        lua_pushnil(L);
        return 1;
    }
    PushSavestateInfo(L, result.Unwrap());
    return 1;
}

static int GetDirectory(lua_State *L) {
    PushString(L, RequireManager(L, "tas.savestate.get_directory")->GetSavestatesDirectory());
    return 1;
}

void LuaApi::RegisterSavestateApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);
    lua_getglobal(state, "tas");

    lua_newtable(state);
    SetClosure(state, "save", Save, context);
    SetClosure(state, "load", Load, context);
    SetClosure(state, "remove", Remove, context);
    SetAlias(state, "del", "remove"); // deprecated
    SetClosure(state, "list", List, context);
    SetClosure(state, "exists", Exists, context);
    SetClosure(state, "get_info", GetInfo, context);
    SetClosure(state, "get_directory", GetDirectory, context);
    lua_setfield(state, -2, "savestate");

    lua_pop(state, 1);
}
