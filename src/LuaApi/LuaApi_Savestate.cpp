#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"

#include "LuaScheduler.h"
#include "SavestateManager.h"
#include "ScriptContext.h"

namespace {

ScriptContext *GetContext(lua_State *L) {
    return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

SavestateManager *GetManager(lua_State *L) {
    auto *context = GetContext(L);
    return context ? context->GetSavestateManager() : nullptr;
}

void PushErrorOrNil(lua_State *L, const Result<void> &result) {
    if (result.IsOk()) {
        lua_pushnil(L);
        return;
    }
    const auto &error = result.GetError();
    lua_pushlstring(L, error.message.data(), error.message.size());
}

void PushVectorTable(lua_State *L, const VxVector &value) {
    lua_newtable(L);
    lua_pushnumber(L, value.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, value.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, value.z);
    lua_setfield(L, -2, "z");
}

void PushSavestateInfo(lua_State *L, const SavestateData &data) {
    lua_newtable(L);
    lua_pushlstring(L, data.name.data(), data.name.size());
    lua_setfield(L, -2, "name");
    lua_pushlstring(L, data.timestamp.data(), data.timestamp.size());
    lua_setfield(L, -2, "timestamp");
    lua_pushlstring(L, data.levelName.data(), data.levelName.size());
    lua_setfield(L, -2, "level_name");
    lua_pushinteger(L, data.levelNumber);
    lua_setfield(L, -2, "level_number");
    lua_pushlstring(L, data.description.data(), data.description.size());
    lua_setfield(L, -2, "description");

    PushVectorTable(L, data.position);
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

void SetSavestateFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, function, 1);
    lua_setfield(L, -2, name);
}

int Save(lua_State *L) {
    auto *manager = GetManager(L);
    if (!manager) {
        lua_pushstring(L, "SavestateManager not available");
        return 1;
    }
    const char *name = luaL_checkstring(L, 1);
    const char *description = lua_gettop(L) >= 2 && !lua_isnil(L, 2) ? luaL_checkstring(L, 2) : "";
    PushErrorOrNil(L, manager->SaveState(name ? name : "", description ? description : ""));
    return 1;
}

int Load(lua_State *L) {
    auto *context = GetContext(L);
    auto *manager = context ? context->GetSavestateManager() : nullptr;
    if (!manager) {
        return luaL_error(L, "SavestateManager not available");
    }

    const char *name = luaL_checkstring(L, 1);
    auto result = manager->LoadState(name ? name : "");
    if (result.IsError()) {
        return luaL_error(L, "Failed to load savestate: %s", result.GetError().message.c_str());
    }

    if (auto *scheduler = context->GetScheduler()) {
        scheduler->YieldTicks(5);
        return lua_yieldk(L, 0, 0, nullptr);
    }
    return 0;
}

int Delete(lua_State *L) {
    auto *manager = GetManager(L);
    if (!manager) {
        lua_pushstring(L, "SavestateManager not available");
        return 1;
    }
    const char *name = luaL_checkstring(L, 1);
    PushErrorOrNil(L, manager->DeleteState(name ? name : ""));
    return 1;
}

int List(lua_State *L) {
    auto *manager = GetManager(L);
    if (!manager) {
        lua_pushnil(L);
        return 1;
    }

    auto result = manager->ListStates();
    if (result.IsError()) {
        lua_pushnil(L);
        return 1;
    }

    const auto states = result.Unwrap();
    lua_newtable(L);
    int index = 1;
    for (const auto &name : states) {
        lua_pushlstring(L, name.data(), name.size());
        lua_seti(L, -2, index++);
    }
    return 1;
}

int Exists(lua_State *L) {
    auto *manager = GetManager(L);
    const char *name = luaL_checkstring(L, 1);
    lua_pushboolean(L, manager && manager->StateExists(name ? name : ""));
    return 1;
}

int GetInfo(lua_State *L) {
    auto *manager = GetManager(L);
    if (!manager) {
        lua_pushnil(L);
        return 1;
    }

    const char *name = luaL_checkstring(L, 1);
    auto result = manager->GetStateInfo(name ? name : "");
    if (result.IsError()) {
        lua_pushnil(L);
        return 1;
    }
    PushSavestateInfo(L, result.Unwrap());
    return 1;
}

int GetDirectory(lua_State *L) {
    auto *manager = GetManager(L);
    std::string directory = manager ? manager->GetSavestatesDirectory() : "";
    lua_pushlstring(L, directory.data(), directory.size());
    return 1;
}

} // namespace

void LuaApi::RegisterSavestateApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);

    lua_getglobal(state, "tas");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_setglobal(state, "tas");
        lua_getglobal(state, "tas");
    }

    lua_newtable(state);
    SetSavestateFunction(state, "save", Save, context);
    SetSavestateFunction(state, "load", Load, context);
    SetSavestateFunction(state, "del", Delete, context);
    lua_getfield(state, -1, "del");
    lua_setfield(state, -2, "remove");
    SetSavestateFunction(state, "list", List, context);
    SetSavestateFunction(state, "exists", Exists, context);
    SetSavestateFunction(state, "get_info", GetInfo, context);
    SetSavestateFunction(state, "get_directory", GetDirectory, context);
    lua_setfield(state, -2, "savestate");

    lua_pop(state, 1);
}
