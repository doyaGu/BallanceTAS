#include "LuaApi.h"

#include "LuaApiCommon.h"
#include "Logger.h"
#include "ScriptContext.h"
#include "ScriptFrameTime.h"
#include "TASEngine.h"
#include "TASProject.h"

using namespace LuaApiCommon;

enum class LogLevel { Info, Warn, Error };

// tas.log(message) or tas.log(format, ...) with string.format semantics.
static int TasLogAt(lua_State *L, LogLevel level, const char *where) {
    const int argc = lua_gettop(L);
    if (argc < 1 || !lua_isstring(L, 1)) {
        return RaiseArg(L, where, 1, "format string expected");
    }

    if (argc > 1) {
        lua_getglobal(L, "string");
        lua_getfield(L, -1, "format");
        lua_remove(L, -2);
        lua_insert(L, 1);
        lua_call(L, argc, 1);
    }

    ScriptContext *context = GetContext(L);
    const char *message = lua_tostring(L, -1);
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

static int TasLog(lua_State *L) {
    return TasLogAt(L, LogLevel::Info, "tas.log");
}

static int TasWarn(lua_State *L) {
    return TasLogAt(L, LogLevel::Warn, "tas.warn");
}

static int TasError(lua_State *L) {
    return TasLogAt(L, LogLevel::Error, "tas.error");
}

static int TasGetTick(lua_State *L) {
    ScriptContext *context = GetContext(L);
    lua_pushinteger(L, context ? static_cast<lua_Integer>(context->GetCurrentTick()) : 0);
    return 1;
}

// tas.get_manifest() -> manifest table | nil (not a script project)
static int TasGetManifest(lua_State *L) {
    ScriptContext *context = GetContext(L);
    const TASProject *project = context ? context->GetCurrentProject() : nullptr;
    if (!project || !project->IsScriptProject()) {
        lua_pushnil(L);
        return 1;
    }
    project->GetManifestTable().Push(L);
    return 1;
}

static ScriptFrameTime *GetFrameTime(lua_State *L) {
    ScriptContext *context = GetContext(L);
    TASEngine *engine = context ? context->GetEngine() : nullptr;
    return engine ? engine->GetServiceProvider().Resolve<ScriptFrameTime>() : nullptr;
}

// tas.set_frame_time(ms) -> true | false, reason
// The game time each frame of a script playback lasts, from the next frame on.
static int TasSetFrameTime(lua_State *L) {
    const lua_Number ms = luaL_checknumber(L, 1);
    if (!(ms > 0.0) || ms > 1000.0) {
        return RaiseArg(L, "tas.set_frame_time", 1, "a frame time in ms, above 0 and at most 1000");
    }
    ScriptFrameTime *frameTime = GetFrameTime(L);
    if (!frameTime || !frameTime->Set(static_cast<float>(ms))) {
        return PushFail(L, "no script playback is running");
    }
    return PushOk(L);
}

// tas.get_frame_time() -> ms | nil (no script playback)
static int TasGetFrameTime(lua_State *L) {
    ScriptFrameTime *frameTime = GetFrameTime(L);
    if (!frameTime || !frameTime->IsActive()) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, frameTime->Get());
    }
    return 1;
}

void LuaApi::RegisterCoreApi(lua_State *state, ScriptContext *context) {
    lua_getglobal(state, "tas");
    SetClosure(state, "log", TasLog, context);
    SetClosure(state, "warn", TasWarn, context);
    SetClosure(state, "error", TasError, context);
    SetClosure(state, "get_tick", TasGetTick, context);
    SetClosure(state, "get_manifest", TasGetManifest, context);
    SetClosure(state, "set_frame_time", TasSetFrameTime, context);
    SetClosure(state, "get_frame_time", TasGetFrameTime, context);
    lua_pop(state, 1);
}
