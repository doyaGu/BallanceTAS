#include "LuaApi.h"

#include "Logger.h"
#include "ScriptContext.h"
#include "TASProject.h"

namespace {

ScriptContext *GetContext(lua_State *state) {
    return static_cast<ScriptContext *>(lua_touserdata(state, lua_upvalueindex(1)));
}

void SetTasFunction(lua_State *state, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_getglobal(state, "tas");
    lua_pushlightuserdata(state, context);
    lua_pushcclosure(state, function, 1);
    lua_setfield(state, -2, name);
    lua_pop(state, 1);
}

int TasLog(lua_State *state) {
    ScriptContext *context = GetContext(state);
    if (lua_gettop(state) != 1 || !lua_isstring(state, 1)) {
        return luaL_error(state, "tas.log(message): expected one string argument");
    }

    const char *message = lua_tostring(state, 1);
    Log::Info("[lua:%s] %s", context ? context->GetName().c_str() : "unknown", message ? message : "");
    return 0;
}

int TasGetTick(lua_State *state) {
    ScriptContext *context = GetContext(state);
    if (lua_gettop(state) != 0) {
        return luaL_error(state, "tas.get_tick(): expected no arguments");
    }

    lua_pushinteger(state, context ? static_cast<lua_Integer>(context->GetCurrentTick()) : 0);
    return 1;
}

int TasGetManifest(lua_State *state) {
    ScriptContext *context = GetContext(state);
    if (lua_gettop(state) != 0) {
        return luaL_error(state, "tas.get_manifest(): expected no arguments");
    }

    const TASProject *project = context ? context->GetCurrentProject() : nullptr;
    if (!project || !project->IsScriptProject()) {
        lua_pushnil(state);
        return 1;
    }

    project->GetManifestTable().Push(state);
    return 1;
}

} // namespace

void LuaApi::RegisterCoreApi(lua_State *state, ScriptContext *context) {
    SetTasFunction(state, "log", TasLog, context);
    SetTasFunction(state, "get_tick", TasGetTick, context);
    SetTasFunction(state, "get_manifest", TasGetManifest, context);
}
