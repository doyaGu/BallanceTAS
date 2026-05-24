#include "LuaApi.h"

#include "../LuaRuntime/LuaFunction.h"
#include "../LuaRuntime/LuaStackGuard.h"

#include "GameInterface.h"
#include "ScriptContext.h"

#include <chrono>

namespace {

ScriptContext *GetContext(lua_State *L) {
    return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

void SetDebugFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, function, 1);
    lua_setfield(L, -2, name);
}

double CurrentMemoryKB(lua_State *L) {
    const int kb = lua_gc(L, LUA_GCCOUNT, 0);
    const int bytes = lua_gc(L, LUA_GCCOUNTB, 0);
    return static_cast<double>(kb) + static_cast<double>(bytes) / 1024.0;
}

int Assert(lua_State *L) {
    const bool condition = lua_toboolean(L, 1) != 0;
    if (condition) {
        return 0;
    }
    const char *message = lua_gettop(L) >= 2 ? lua_tostring(L, 2) : nullptr;
    return luaL_error(L, "%s", message ? message : "Assertion failed!");
}

int SkipRendering(lua_State *L) {
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    const lua_Integer ticks = luaL_checkinteger(L, 1);
    if (game && ticks > 0) {
        game->SkipRenderForTicks(static_cast<size_t>(ticks));
    }
    return 0;
}

int GetStackTrace(lua_State *L) {
    const int maxDepth = lua_gettop(L) >= 1 && !lua_isnil(L, 1) ? static_cast<int>(luaL_checkinteger(L, 1)) : 20;
    lua_newtable(L);

    for (int level = 1; level <= maxDepth; ++level) {
        lua_Debug ar;
        if (lua_getstack(L, level, &ar) == 0) {
            break;
        }

        lua_getinfo(L, "nSl", &ar);
        lua_newtable(L);
        lua_pushinteger(L, level);
        lua_setfield(L, -2, "level");
        lua_pushstring(L, ar.name ? ar.name : "<unknown>");
        lua_setfield(L, -2, "name");
        lua_pushstring(L, ar.source ? ar.source : "<unknown>");
        lua_setfield(L, -2, "source");
        lua_pushstring(L, ar.short_src);
        lua_setfield(L, -2, "short_src");
        lua_pushinteger(L, ar.currentline);
        lua_setfield(L, -2, "line");
        lua_pushstring(L, ar.what ? ar.what : "?");
        lua_setfield(L, -2, "what");
        lua_seti(L, -2, level);
    }
    return 1;
}

int MemorySnapshot(lua_State *L) {
    const double memoryKB = CurrentMemoryKB(L);
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    lua_newtable(L);
    lua_pushnumber(L, memoryKB);
    lua_setfield(L, -2, "total_kb");
    lua_pushinteger(L, static_cast<lua_Integer>(memoryKB * 1024.0));
    lua_setfield(L, -2, "total_bytes");
    lua_pushinteger(L, static_cast<lua_Integer>(ms));
    lua_setfield(L, -2, "timestamp");
    return 1;
}

double ReadTableNumber(lua_State *L, int index, const char *field) {
    index = lua_absindex(L, index);
    lua_getfield(L, index, field);
    const double value = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0;
    lua_pop(L, 1);
    return value;
}

int MemoryDiff(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);
    const double kb1 = ReadTableNumber(L, 1, "total_kb");
    const double kb2 = ReadTableNumber(L, 2, "total_kb");
    const double delta = kb2 - kb1;

    lua_newtable(L);
    lua_pushnumber(L, delta);
    lua_setfield(L, -2, "delta_kb");
    lua_pushnumber(L, delta * 1024.0);
    lua_setfield(L, -2, "delta_bytes");
    lua_pushnumber(L, kb1 > 0.0 ? (delta / kb1) * 100.0 : 0.0);
    lua_setfield(L, -2, "percentage");
    return 1;
}

int Profile(lua_State *L) {
    if (!lua_isfunction(L, 1)) {
        return luaL_error(L, "tas.profile(function): expected function");
    }

    const double memoryBefore = CurrentMemoryKB(L);
    const auto start = std::chrono::high_resolution_clock::now();
    lua_pushvalue(L, 1);
    tas::lua::LuaFunction function = tas::lua::LuaFunction::FromStack(L, -1);
    auto result = function.Call(0, 0);
    const auto end = std::chrono::high_resolution_clock::now();
    const double memoryAfter = CurrentMemoryKB(L);
    const auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    lua_newtable(L);
    lua_pushinteger(L, static_cast<lua_Integer>(durationUs));
    lua_setfield(L, -2, "duration_us");
    lua_pushnumber(L, static_cast<double>(durationUs) / 1000.0);
    lua_setfield(L, -2, "duration_ms");
    lua_pushnumber(L, memoryBefore);
    lua_setfield(L, -2, "memory_before_kb");
    lua_pushnumber(L, memoryAfter);
    lua_setfield(L, -2, "memory_after_kb");
    lua_pushnumber(L, memoryAfter - memoryBefore);
    lua_setfield(L, -2, "memory_delta_kb");
    lua_pushboolean(L, result.IsOk());
    lua_setfield(L, -2, "success");
    if (result.IsError()) {
        const auto &error = result.GetError();
        lua_pushlstring(L, error.message.data(), error.message.size());
        lua_setfield(L, -2, "error");
    }
    return 1;
}

int ForceGC(lua_State *L) {
    lua_pushinteger(L, lua_gc(L, LUA_GCCOLLECT, 0));
    return 1;
}

int GetMemoryUsage(lua_State *L) {
    lua_pushnumber(L, CurrentMemoryKB(L));
    return 1;
}

} // namespace

void LuaApi::RegisterDebugApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);

    lua_getglobal(state, "tas");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_setglobal(state, "tas");
        lua_getglobal(state, "tas");
    }

    SetDebugFunction(state, "assert", Assert, context);
    SetDebugFunction(state, "skip_rendering", SkipRendering, context);
    SetDebugFunction(state, "get_stack_trace", GetStackTrace, context);
    SetDebugFunction(state, "memory_snapshot", MemorySnapshot, context);
    SetDebugFunction(state, "memory_diff", MemoryDiff, context);
    SetDebugFunction(state, "profile", Profile, context);
    SetDebugFunction(state, "force_gc", ForceGC, context);
    SetDebugFunction(state, "get_memory_usage", GetMemoryUsage, context);

    lua_pop(state, 1);
}
