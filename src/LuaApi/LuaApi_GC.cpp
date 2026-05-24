#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"

#include "ScriptContext.h"

#include <cstring>

static ScriptContext *GetContext(lua_State *L) {
    return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

static void SetGCFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, function, 1);
    lua_setfield(L, -2, name);
}

static const char *GCModeName(LuaGCMode mode) {
    return mode == LuaGCMode::Generational ? "generational" : "incremental";
}

static int Collect(lua_State *L) {
    lua_gc(L, LUA_GCCOLLECT, 0);
    return 0;
}

static int Stop(lua_State *L) {
    lua_gc(L, LUA_GCSTOP, 0);
    return 0;
}

static int Restart(lua_State *L) {
    lua_gc(L, LUA_GCRESTART, 0);
    return 0;
}

static int Step(lua_State *L) {
    const int stepSize = lua_gettop(L) >= 1 && !lua_isnil(L, 1) ? static_cast<int>(luaL_checkinteger(L, 1)) : 1;
    if (stepSize < 0) {
        return luaL_error(L, "gc.step: step_size must be non-negative");
    }
    lua_pushboolean(L, lua_gc(L, LUA_GCSTEP, stepSize) != 0);
    return 1;
}

static int SetMode(lua_State *L) {
    auto *context = GetContext(L);
    if (!context) {
        return luaL_error(L, "gc.set_mode: context unavailable");
    }

    const char *mode = luaL_checkstring(L, 1);
    if (std::strcmp(mode, "generational") == 0) {
        lua_pushboolean(L, context->SetGCMode(LuaGCMode::Generational));
        return 1;
    }
    if (std::strcmp(mode, "incremental") == 0) {
        lua_pushboolean(L, context->SetGCMode(LuaGCMode::Incremental));
        return 1;
    }
    return luaL_error(L, "gc.set_mode: mode must be 'generational' or 'incremental'");
}

static int GetMode(lua_State *L) {
    auto *context = GetContext(L);
    lua_pushstring(L, context ? GCModeName(context->GetGCMode()) : "incremental");
    return 1;
}

static int Tune(lua_State *L) {
    if (lua_gettop(L) != 1 || !lua_istable(L, 1)) {
        return luaL_error(L, "gc.tune(params): expected table");
    }

    lua_newtable(L);

    lua_getfield(L, 1, "pause");
    if (!lua_isnil(L, -1)) {
        const int value = static_cast<int>(luaL_checkinteger(L, -1));
        if (value < 0) {
            return luaL_error(L, "gc.tune: pause must be non-negative");
        }
#if defined(LUA_GCPARAM)
        const int old = lua_gc(L, LUA_GCPARAM, LUA_GCPPAUSE, value);
#else
        const int old = lua_gc(L, LUA_GCSETPAUSE, value);
#endif
        lua_pushinteger(L, old);
        lua_setfield(L, -3, "old_pause");
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "stepmul");
    if (!lua_isnil(L, -1)) {
        const int value = static_cast<int>(luaL_checkinteger(L, -1));
        if (value < 0) {
            return luaL_error(L, "gc.tune: stepmul must be non-negative");
        }
#if defined(LUA_GCPARAM)
        const int old = lua_gc(L, LUA_GCPARAM, LUA_GCPSTEPMUL, value);
#else
        const int old = lua_gc(L, LUA_GCSETSTEPMUL, value);
#endif
        lua_pushinteger(L, old);
        lua_setfield(L, -3, "old_stepmul");
    }
    lua_pop(L, 1);

    return 1;
}

static int MemoryKB(lua_State *L) {
    auto *context = GetContext(L);
    lua_pushnumber(L, context ? context->GetLuaMemoryKB() : 0.0);
    return 1;
}

static int MemoryBytes(lua_State *L) {
    auto *context = GetContext(L);
    lua_pushinteger(L, context ? static_cast<lua_Integer>(context->GetLuaMemoryBytes()) : 0);
    return 1;
}

static int IsRunning(lua_State *L) {
#if LUA_VERSION_NUM >= 502
    lua_pushboolean(L, lua_gc(L, LUA_GCISRUNNING, 0) != 0);
#else
    lua_pushboolean(L, 1);
#endif
    return 1;
}

static int Stats(lua_State *L) {
    auto *context = GetContext(L);
    const int memoryKB = lua_gc(L, LUA_GCCOUNT, 0);
    const int memoryBytesRemainder = lua_gc(L, LUA_GCCOUNTB, 0);

    lua_newtable(L);
    lua_pushnumber(L, static_cast<double>(memoryKB) + static_cast<double>(memoryBytesRemainder) / 1024.0);
    lua_setfield(L, -2, "memory_kb");
    lua_pushinteger(L, static_cast<lua_Integer>(memoryKB) * 1024 + memoryBytesRemainder);
    lua_setfield(L, -2, "memory_bytes");
    lua_pushstring(L, context ? GCModeName(context->GetGCMode()) : "incremental");
    lua_setfield(L, -2, "mode");
#if LUA_VERSION_NUM >= 502
    lua_pushboolean(L, lua_gc(L, LUA_GCISRUNNING, 0) != 0);
#else
    lua_pushboolean(L, 1);
#endif
    lua_setfield(L, -2, "running");
    lua_pushstring(L, context ? context->GetName().c_str() : "");
    lua_setfield(L, -2, "context_name");

    const char *contextType = "unknown";
    if (context) {
        switch (context->GetType()) {
        case ScriptContextType::Global:
            contextType = "global";
            break;
        case ScriptContextType::Level:
            contextType = "level";
            break;
        case ScriptContextType::Custom:
            contextType = "custom";
            break;
        }
    }
    lua_pushstring(L, contextType);
    lua_setfield(L, -2, "context_type");
    return 1;
}

void LuaApi::RegisterGCApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);

    lua_getglobal(state, "tas");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_setglobal(state, "tas");
        lua_getglobal(state, "tas");
    }

    lua_newtable(state);
    SetGCFunction(state, "collect", Collect, context);
    SetGCFunction(state, "stop", Stop, context);
    SetGCFunction(state, "restart", Restart, context);
    SetGCFunction(state, "step", Step, context);
    SetGCFunction(state, "set_mode", SetMode, context);
    SetGCFunction(state, "get_mode", GetMode, context);
    SetGCFunction(state, "tune", Tune, context);
    SetGCFunction(state, "stats", Stats, context);
    SetGCFunction(state, "get_memory_kb", MemoryKB, context);
    SetGCFunction(state, "get_memory_bytes", MemoryBytes, context);
    SetGCFunction(state, "is_running", IsRunning, context);
    lua_setfield(state, -2, "gc");

    lua_pop(state, 1);
}
