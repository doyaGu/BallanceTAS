#include "LuaApi.h"

#include "LuaApiCommon.h"
#include "LuaRuntime/LuaStackGuard.h"

#include "ScriptContext.h"

#include <cstring>

using namespace LuaApiCommon;

static const char *GCModeName(LuaGCMode mode) {
    return mode == LuaGCMode::Generational ? "generational" : "incremental";
}

static const char *ContextTypeName(const ScriptContext *context) {
    if (!context) {
        return "unknown";
    }
    switch (context->GetType()) {
    case ScriptContextType::Global:
        return "global";
    case ScriptContextType::Level:
        return "level";
    case ScriptContextType::Custom:
        return "custom";
    }
    return "unknown";
}

static double MemoryKBNow(lua_State *L) {
    return static_cast<double>(lua_gc(L, LUA_GCCOUNT, 0)) + static_cast<double>(lua_gc(L, LUA_GCCOUNTB, 0)) / 1024.0;
}

static bool CollectorRunning(lua_State *L) {
    return lua_gc(L, LUA_GCISRUNNING, 0) != 0;
}

// tas.gc.collect() -> KB freed by a full collection
static int Collect(lua_State *L) {
    const double before = MemoryKBNow(L);
    lua_gc(L, LUA_GCCOLLECT, 0);
    lua_pushnumber(L, before - MemoryKBNow(L));
    return 1;
}

static int Stop(lua_State *L) {
    lua_gc(L, LUA_GCSTOP, 0);
    return 0;
}

static int Restart(lua_State *L) {
    lua_gc(L, LUA_GCRESTART, 0);
    return 0;
}

// tas.gc.step([size]) -> true when a cycle finished
static int Step(lua_State *L) {
    const int size = static_cast<int>(OptIndex(L, 1, "tas.gc.step", 1));
    lua_pushboolean(L, lua_gc(L, LUA_GCSTEP, size) != 0);
    return 1;
}

// tas.gc.set_mode("generational" | "incremental") -> true | false, reason
static int SetMode(lua_State *L) {
    const char *mode = luaL_checkstring(L, 1);
    LuaGCMode target = LuaGCMode::Incremental;
    if (std::strcmp(mode, "generational") == 0) {
        target = LuaGCMode::Generational;
    } else if (std::strcmp(mode, "incremental") != 0) {
        return RaiseArg(L, "tas.gc.set_mode", 1, "'generational' or 'incremental' expected");
    }
    ScriptContext *context = GetContext(L);
    if (!context) {
        return Raise(L, "tas.gc.set_mode", "the script context is not available");
    }
    return context->SetGCMode(target) ? PushOk(L) : PushFail(L, "the collector rejected the mode");
}

static int GetMode(lua_State *L) {
    ScriptContext *context = GetContext(L);
    lua_pushstring(L, context ? GCModeName(context->GetGCMode()) : "incremental");
    return 1;
}

// tas.gc.tune({pause=, stepmul=}) -> {old_pause=, old_stepmul=}
static int Tune(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_newtable(L);
    struct Param {
        const char *field;
        const char *oldField;
        int code;
    };
    const Param params[] = {
        {"pause", "old_pause", LUA_GCPPAUSE},
        {"stepmul", "old_stepmul", LUA_GCPSTEPMUL},
    };
    for (const Param &param : params) {
        lua_getfield(L, 1, param.field);
        if (!lua_isnil(L, -1)) {
            int isInteger = 0;
            const lua_Integer value = lua_tointegerx(L, -1, &isInteger);
            if (!isInteger || value < 0) {
                return Raise(L, "tas.gc.tune", "%s must be a non-negative integer", param.field);
            }
            lua_pushinteger(L, lua_gc(L, LUA_GCPARAM, param.code, static_cast<int>(value)));
            lua_setfield(L, -3, param.oldField);
        }
        lua_pop(L, 1);
    }
    return 1;
}

static int MemoryKB(lua_State *L) {
    lua_pushnumber(L, MemoryKBNow(L));
    return 1;
}

static int MemoryBytes(lua_State *L) {
    lua_pushinteger(L, static_cast<lua_Integer>(lua_gc(L, LUA_GCCOUNT, 0)) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0));
    return 1;
}

static int IsRunning(lua_State *L) {
    lua_pushboolean(L, CollectorRunning(L));
    return 1;
}

static int Stats(lua_State *L) {
    ScriptContext *context = GetContext(L);
    lua_newtable(L);
    lua_pushnumber(L, MemoryKBNow(L));
    lua_setfield(L, -2, "memory_kb");
    lua_pushinteger(L, static_cast<lua_Integer>(lua_gc(L, LUA_GCCOUNT, 0)) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0));
    lua_setfield(L, -2, "memory_bytes");
    lua_pushstring(L, context ? GCModeName(context->GetGCMode()) : "incremental");
    lua_setfield(L, -2, "mode");
    lua_pushboolean(L, CollectorRunning(L));
    lua_setfield(L, -2, "running");
    PushString(L, context ? context->GetName() : std::string());
    lua_setfield(L, -2, "context_name");
    lua_pushstring(L, ContextTypeName(context));
    lua_setfield(L, -2, "context_type");
    return 1;
}

void LuaApi::RegisterGCApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);
    lua_getglobal(state, "tas");
    lua_newtable(state);
    SetClosure(state, "collect", Collect, context);
    SetClosure(state, "stop", Stop, context);
    SetClosure(state, "restart", Restart, context);
    SetClosure(state, "step", Step, context);
    SetClosure(state, "set_mode", SetMode, context);
    SetClosure(state, "get_mode", GetMode, context);
    SetClosure(state, "tune", Tune, context);
    SetClosure(state, "stats", Stats, context);
    SetClosure(state, "get_memory_kb", MemoryKB, context);
    SetClosure(state, "get_memory_bytes", MemoryBytes, context);
    SetClosure(state, "is_running", IsRunning, context);
    // Deprecated top-level shortcuts, the same function objects.
    lua_getfield(state, -1, "collect");
    lua_setfield(state, -3, "force_gc");
    lua_getfield(state, -1, "get_memory_kb");
    lua_setfield(state, -3, "get_memory_usage");
    lua_setfield(state, -2, "gc");
    lua_pop(state, 1);
}
