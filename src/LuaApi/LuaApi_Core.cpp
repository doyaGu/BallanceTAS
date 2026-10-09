#include "LuaApi.h"

#include "Logger.h"
#include "ScriptContext.h"
#include "TASProject.h"

static ScriptContext *GetContext(lua_State *state) {
    return static_cast<ScriptContext *>(lua_touserdata(state, lua_upvalueindex(1)));
}

static void SetTasFunction(lua_State *state, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_getglobal(state, "tas");
    lua_pushlightuserdata(state, context);
    lua_pushcclosure(state, function, 1);
    lua_setfield(state, -2, name);
    lua_pop(state, 1);
}

enum class LogLevel { Info, Warn, Error };

// tas.log(message) or tas.log(format, ...) with string.format semantics.
static int TasLogAt(lua_State *state, LogLevel level, const char *functionName) {
    ScriptContext *context = GetContext(state);
    const int argc = lua_gettop(state);
    if (argc < 1 || !lua_isstring(state, 1)) {
        return luaL_error(state, "%s(format, ...): expected a format string", functionName);
    }

    if (argc > 1) {
        lua_getglobal(state, "string");
        lua_getfield(state, -1, "format");
        lua_remove(state, -2);
        lua_insert(state, 1);
        lua_call(state, argc, 1);
    }

    const char *message = lua_tostring(state, -1);
    const char *name = context ? context->GetName().c_str() : "unknown";
    switch (level) {
    case LogLevel::Info:
        Log::Info("[lua:%s] %s", name, message ? message : "");
        break;
    case LogLevel::Warn:
        Log::Warn("[lua:%s] %s", name, message ? message : "");
        break;
    case LogLevel::Error:
        Log::Error("[lua:%s] %s", name, message ? message : "");
        break;
    }
    return 0;
}

static int TasLog(lua_State *state) {
    return TasLogAt(state, LogLevel::Info, "tas.log");
}

static int TasWarn(lua_State *state) {
    return TasLogAt(state, LogLevel::Warn, "tas.warn");
}

static int TasError(lua_State *state) {
    return TasLogAt(state, LogLevel::Error, "tas.error");
}

static int TasGetTick(lua_State *state) {
    ScriptContext *context = GetContext(state);
    if (lua_gettop(state) != 0) {
        return luaL_error(state, "tas.get_tick(): expected no arguments");
    }

    lua_pushinteger(state, context ? static_cast<lua_Integer>(context->GetCurrentTick()) : 0);
    return 1;
}

static int TasGetManifest(lua_State *state) {
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

void LuaApi::RegisterCoreApi(lua_State *state, ScriptContext *context) {
    SetTasFunction(state, "log", TasLog, context);
    SetTasFunction(state, "warn", TasWarn, context);
    SetTasFunction(state, "error", TasError, context);
    SetTasFunction(state, "get_tick", TasGetTick, context);
    SetTasFunction(state, "get_manifest", TasGetManifest, context);
}
