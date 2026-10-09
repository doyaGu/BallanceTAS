#include "LuaApi.h"

#include "LuaApiCommon.h"
#include "LuaRuntime/LuaFunction.h"
#include "LuaScheduler.h"
#include "ScriptContext.h"

using namespace LuaApiCommon;

// The scheduler comes from upvalue 2 when registered with one (tests), else from the context.
static LuaScheduler *RequireWaitScheduler(lua_State *L, const char *where) {
    LuaScheduler *scheduler = nullptr;
    if (lua_islightuserdata(L, lua_upvalueindex(2))) {
        scheduler = static_cast<LuaScheduler *>(lua_touserdata(L, lua_upvalueindex(2)));
    }
    if (!scheduler) {
        return RequireYieldableScheduler(L, where);
    }
    if (!lua_isyieldable(L) || !scheduler->CanYieldCurrentThread()) {
        Raise(L, where, "waits, so it must be called from a script coroutine (main, a task or a listener)");
    }
    return scheduler;
}

// tas.wait_ticks(ticks)
static int TasWaitTicks(lua_State *L) {
    const int ticks = CheckTicks(L, 1, "tas.wait_ticks");
    RequireWaitScheduler(L, "tas.wait_ticks")->YieldTicks(ticks);
    return lua_yieldk(L, 0, 0, nullptr);
}

// tas.wait_event(name)
static int TasWaitEvent(lua_State *L) {
    size_t length = 0;
    const char *name = CheckNonEmptyString(L, 1, "tas.wait_event", &length);
    LuaScheduler *scheduler = RequireWaitScheduler(L, "tas.wait_event");
    scheduler->YieldWaitForEvent(std::string(name, length));
    return lua_yieldk(L, 0, 0, nullptr);
}

// tas.wait_until(predicate)
static int TasWaitUntil(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    LuaScheduler *scheduler = RequireWaitScheduler(L, "tas.wait_until");
    lua_pushvalue(L, 1);
    scheduler->YieldUntil(tas::lua::LuaFunction::FromStack(L, -1));
    return lua_yieldk(L, 0, 0, nullptr);
}

// tas.wait(ticks | event_name | predicate)
static int TasWait(lua_State *L) {
    switch (lua_type(L, 1)) {
    case LUA_TNUMBER:
        return TasWaitTicks(L);
    case LUA_TSTRING:
        return TasWaitEvent(L);
    case LUA_TFUNCTION:
        return TasWaitUntil(L);
    default:
        return RaiseArg(L, "tas.wait", 1, "tick count, event name or predicate expected");
    }
}

static void SetWaitFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context,
                            LuaScheduler *scheduler) {
    lua_pushlightuserdata(L, context);
    lua_pushlightuserdata(L, scheduler);
    lua_pushcclosure(L, function, 2);
    lua_setfield(L, -2, name);
}

void LuaApi::RegisterConcurrencyApi(lua_State *state, ScriptContext *context, LuaScheduler *scheduler) {
    lua_getglobal(state, "tas");
    SetWaitFunction(state, "wait_ticks", TasWaitTicks, context, scheduler);
    SetWaitFunction(state, "wait_event", TasWaitEvent, context, scheduler);
    SetWaitFunction(state, "wait_until", TasWaitUntil, context, scheduler);
    SetWaitFunction(state, "wait", TasWait, context, scheduler);
    lua_pop(state, 1);
}
