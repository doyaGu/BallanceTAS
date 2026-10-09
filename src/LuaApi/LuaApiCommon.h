#pragma once

// Shared helpers for the Lua bindings, and the conventions every binding follows:
//
// 1. Caller mistakes raise: a wrong argument type or range, calling a waiting function
//    outside a scheduler coroutine, or a missing host service. Messages read
//    "tas.<module>.<fn>: <message>" (classes: "<Class>:<method>: <message>").
// 2. Expected failures (game state, files, "not found" when acting) are return values:
//    - actions:   true | false, reason      (PushOk / PushFail / PushResult)
//    - producers: value | nil, reason
//    - lookups:   value | nil               (absent is not an error)
//    All three work with assert(...).
// 3. No exact-arity checks: extra arguments are ignored, optional arguments accept nil.
// 4. Functions that wait say so in their name, and check yieldability before any side
//    effect (RequireYieldableScheduler).
// 5. Stream positions (record frames, byte offsets) and vector components are 0-based;
//    lists are 1-based; ids are opaque. Script time is in ticks, angles in radians.
//
// Value helpers (math types, game objects) are in LuaApiValues.h.
//
// Nothing here may own C++ resources across a raise: Lua is built as C, so luaL_error
// longjmps past destructors. Validate arguments before building std::string & co.

#include <string>
#include <vector>

#include "LuaRuntime/LuaHeaders.h"

class LuaScheduler;
class ScriptContext;

namespace LuaApiCommon {
    // Registry key holding the ScriptContext of the VM, for bindings without upvalues.
    constexpr const char *kContextRegistryKey = "BallanceTAS.Context";

    // ---- Context and registration ---------------------------------------------------

    // ScriptContext stored as upvalue 1 of the running C closure.
    inline ScriptContext *GetContext(lua_State *L) {
        return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
    }

    // ScriptContext of the VM, for class methods (which have no context upvalue).
    ScriptContext *GetRegistryContext(lua_State *L);

    // t[name] = closure(fn, upvalue) on the table at the top of the stack.
    inline void SetClosure(lua_State *L, const char *name, lua_CFunction fn, void *upvalue) {
        lua_pushlightuserdata(L, upvalue);
        lua_pushcclosure(L, fn, 1);
        lua_setfield(L, -2, name);
    }

    // t[name] = fn on the table at the top of the stack.
    inline void SetFunction(lua_State *L, const char *name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    }

    // t[alias] = t[name]: the same function object under a second name.
    inline void SetAlias(lua_State *L, const char *alias, const char *name) {
        lua_getfield(L, -1, name);
        lua_setfield(L, -2, alias);
    }

    // Pushes tas[name] (creating it if needed) on top of the stack.
    void PushTasSubtable(lua_State *L, const char *name);

    // ---- Errors and results -----------------------------------------------------------

    // Raises "<where>: <formatted message>". Never returns.
    int Raise(lua_State *L, const char *where, const char *format, ...);

    // Raises "<where>: bad argument #<arg> (<expected>)". Never returns.
    int RaiseArg(lua_State *L, const char *where, int arg, const char *expected);

    inline int PushOk(lua_State *L) {
        lua_pushboolean(L, 1);
        return 1;
    }

    inline int PushFail(lua_State *L, const char *reason) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, reason);
        return 2;
    }

    inline int PushFail(lua_State *L, const std::string &reason) {
        lua_pushboolean(L, 0);
        lua_pushlstring(L, reason.data(), reason.size());
        return 2;
    }

    inline int PushResult(lua_State *L, bool ok, const std::string &reason) {
        return ok ? PushOk(L) : PushFail(L, reason);
    }

    inline int PushNilReason(lua_State *L, const std::string &reason) {
        lua_pushnil(L);
        lua_pushlstring(L, reason.data(), reason.size());
        return 2;
    }

    // ---- Arguments ----------------------------------------------------------------------

    // Non-empty string argument.
    const char *CheckNonEmptyString(lua_State *L, int arg, const char *where, size_t *length = nullptr);

    // Non-negative integer argument (frame numbers, offsets, counts).
    size_t CheckIndex(lua_State *L, int arg, const char *where);
    size_t OptIndex(lua_State *L, int arg, const char *where, size_t fallback);

    // Strictly positive tick count.
    int CheckTicks(lua_State *L, int arg, const char *where);

    // Optional boolean: nil/none gives the fallback.
    inline bool OptBool(lua_State *L, int arg, bool fallback) {
        return lua_isnoneornil(L, arg) ? fallback : lua_toboolean(L, arg) != 0;
    }

    // ---- Values ------------------------------------------------------------------------------

    inline void PushString(lua_State *L, const std::string &value) {
        lua_pushlstring(L, value.data(), value.size());
    }

    void PushStringList(lua_State *L, const std::vector<std::string> &values);
    void PushIndexList(lua_State *L, const std::vector<size_t> &values);

    // ---- Scheduler ------------------------------------------------------------------------

    // Scheduler of the calling context; raises when missing.
    LuaScheduler *RequireScheduler(lua_State *L, const char *where);

    // As RequireScheduler, and raises unless the caller can wait. Call before side effects.
    LuaScheduler *RequireYieldableScheduler(lua_State *L, const char *where);

    // True when the caller runs in a scheduler coroutine that may wait.
    bool CanYield(lua_State *L);

    // Runs a Lua chunk that installs script-side helpers; raises on failure.
    void RunInstallChunk(lua_State *L, const char *chunk, const char *chunkName, int results = 0);
}
