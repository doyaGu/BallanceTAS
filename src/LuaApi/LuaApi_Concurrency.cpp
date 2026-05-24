#include "LuaApi.h"

#include "LuaRuntime/LuaFunction.h"
#include "LuaScheduler.h"
#include "ScriptContext.h"

static ScriptContext *GetContext(lua_State *state) {
    return static_cast<ScriptContext *>(lua_touserdata(state, lua_upvalueindex(1)));
}

static LuaScheduler *RequireScheduler(lua_State *state) {
    if (lua_islightuserdata(state, lua_upvalueindex(2))) {
        auto *scheduler = static_cast<LuaScheduler *>(lua_touserdata(state, lua_upvalueindex(2)));
        if (scheduler) {
            return scheduler;
        }
    }
    ScriptContext *context = GetContext(state);
    LuaScheduler *scheduler = context ? context->GetScheduler() : nullptr;
    if (!scheduler) {
        luaL_error(state, "scheduler is not available");
    }
    return scheduler;
}

static LuaScheduler *RequireYieldableScheduler(lua_State *state, const char *functionName) {
    LuaScheduler *scheduler = RequireScheduler(state);
    if (!lua_isyieldable(state) || !scheduler->CanYieldCurrentThread()) {
        luaL_error(state, "%s must be called from a scheduler coroutine", functionName);
    }
    return scheduler;
}

static int TasWaitTicks(lua_State *state) {
    if (lua_gettop(state) != 1 || !lua_isinteger(state, 1)) {
        return luaL_error(state, "tas.wait_ticks(ticks): expected one integer argument");
    }

    const int ticks = static_cast<int>(lua_tointeger(state, 1));
    if (ticks <= 0) {
        return luaL_error(state, "tas.wait_ticks: tick count must be positive");
    }

    RequireYieldableScheduler(state, "tas.wait_ticks")->YieldTicks(ticks);
    return lua_yieldk(state, 0, 0, nullptr);
}

static int TasWaitEvent(lua_State *state) {
    if (lua_gettop(state) != 1 || !lua_isstring(state, 1)) {
        return luaL_error(state, "tas.wait_event(event_name): expected one string argument");
    }

    size_t length = 0;
    const char *eventName = lua_tolstring(state, 1, &length);
    if (!eventName || length == 0) {
        return luaL_error(state, "tas.wait_event: event name cannot be empty");
    }

    RequireYieldableScheduler(state, "tas.wait_event")->YieldWaitForEvent(std::string(eventName, length));
    return lua_yieldk(state, 0, 0, nullptr);
}

static int TasWaitUntil(lua_State *state) {
    if (lua_gettop(state) != 1 || !lua_isfunction(state, 1)) {
        return luaL_error(state, "tas.wait_until(predicate): expected one function argument");
    }

    lua_pushvalue(state, 1);
    tas::lua::LuaFunction predicate = tas::lua::LuaFunction::FromStack(state, -1);
    RequireYieldableScheduler(state, "tas.wait_until")->YieldUntil(std::move(predicate));
    return lua_yieldk(state, 0, 0, nullptr);
}

static int TasWait(lua_State *state) {
    if (lua_gettop(state) != 1) {
        return luaL_error(state, "tas.wait(value): expected one argument");
    }

    if (lua_isinteger(state, 1)) {
        return TasWaitTicks(state);
    }
    if (lua_isstring(state, 1)) {
        return TasWaitEvent(state);
    }
    if (lua_isfunction(state, 1)) {
        return TasWaitUntil(state);
    }
    return luaL_error(state, "tas.wait: expected integer ticks, event string, or predicate function");
}

static void SetTasFunction(lua_State *state, const char *name, lua_CFunction function, ScriptContext *context, LuaScheduler *scheduler) {
    lua_getglobal(state, "tas");
    lua_pushlightuserdata(state, context);
    lua_pushlightuserdata(state, scheduler);
    lua_pushcclosure(state, function, 2);
    lua_setfield(state, -2, name);
    lua_pop(state, 1);
}

void LuaApi::RegisterConcurrencyApi(lua_State *state, ScriptContext *context, LuaScheduler *scheduler) {
    SetTasFunction(state, "wait_ticks", TasWaitTicks, context, scheduler);
    SetTasFunction(state, "wait_event", TasWaitEvent, context, scheduler);
    SetTasFunction(state, "wait_until", TasWaitUntil, context, scheduler);
    SetTasFunction(state, "wait", TasWait, context, scheduler);
}
