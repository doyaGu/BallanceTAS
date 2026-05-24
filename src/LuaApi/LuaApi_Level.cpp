#include "LuaApi.h"

#include "../LuaRuntime/LuaFunction.h"
#include "../LuaRuntime/LuaStackGuard.h"

#include "GameInterface.h"
#include "Logger.h"
#ifdef Yield
#undef Yield
#endif
#include "LuaScheduler.h"
#include "ScriptContext.h"

#include <stdexcept>

static ScriptContext *GetContext(lua_State *L) {
    return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

static LuaScheduler *RequireScheduler(lua_State *L, const char *functionName) {
    auto *context = GetContext(L);
    auto *scheduler = context ? context->GetScheduler() : nullptr;
    if (!scheduler) {
        luaL_error(L, "%s: Scheduler not available", functionName);
    }
    return scheduler;
}

static void SetLevelFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, function, 1);
    lua_setfield(L, -2, name);
}

static int GetCurrent(lua_State *L) {
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    const std::string name = game ? game->GetMapName() : "";
    lua_pushlstring(L, name.data(), name.size());
    return 1;
}

static int GetCurrentNumber(lua_State *L) {
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushinteger(L, game ? game->GetCurrentLevel() : 0);
    return 1;
}

static int IsLoaded(lua_State *L) {
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushboolean(L, game && game->IsPlaying());
    return 1;
}

static int IsPaused(lua_State *L) {
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushboolean(L, game && game->IsPaused());
    return 1;
}

static int GetSector(lua_State *L) {
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushinteger(L, game ? game->GetCurrentSector() : 0);
    return 1;
}

static int StubLoad(lua_State *L) {
    size_t length = 0;
    const char *levelName = luaL_checklstring(L, 1, &length);
    Log::Warn("[STUB] level.load('%.*s') - Not yet implemented", static_cast<int>(length), levelName ? levelName : "");
    Log::Info("  Future implementation: This will trigger level loading through the game engine");
    return luaL_error(L, "level.load: Not yet implemented - stub function");
}

static int StubRestart(lua_State *L) {
    Log::Warn("[STUB] level.restart() - Not yet implemented");
    Log::Info("  Future implementation: This will restart the current level");
    return luaL_error(L, "level.restart: Not yet implemented - stub function");
}

static int StubExitToMenu(lua_State *L) {
    Log::Warn("[STUB] level.exit_to_menu() - Not yet implemented");
    Log::Info("  Future implementation: This will exit to the main menu");
    return luaL_error(L, "level.exit_to_menu: Not yet implemented - stub function");
}

static int StubNext(lua_State *L) {
    Log::Warn("[STUB] level.next() - Not yet implemented");
    Log::Info("  Future implementation: This will load the next level in sequence");
    return luaL_error(L, "level.next: Not yet implemented - stub function");
}

static int StubPrevious(lua_State *L) {
    Log::Warn("[STUB] level.previous() - Not yet implemented");
    Log::Info("  Future implementation: This will load the previous level");
    return luaL_error(L, "level.previous: Not yet implemented - stub function");
}

static int IsCompleted(lua_State *L) {
    Log::Warn("[STUB] level.is_completed() - Not yet implemented, returning false");
    Log::Info("  Future implementation: This will check if the level end was reached");
    lua_pushboolean(L, 0);
    return 1;
}

static int IsAtCheckpoint(lua_State *L) {
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    const int sector = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, game && game->GetCurrentSector() == sector);
    return 1;
}

static int IsPlayingPredicate(lua_State *L) {
    auto *context = static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
    auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushboolean(L, game && game->IsPlaying());
    return 1;
}

static int CheckpointPredicate(lua_State *L) {
    auto *context = static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
    const int targetSector = static_cast<int>(lua_tointeger(L, lua_upvalueindex(2)));
    auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushboolean(L, game && game->GetCurrentSector() >= targetSector);
    return 1;
}

static int WaitForLoad(lua_State *L) {
    lua_pushlightuserdata(L, GetContext(L));
    lua_pushcclosure(L, IsPlayingPredicate, 1);
    auto predicate = tas::lua::LuaFunction::FromStack(L, -1);
    RequireScheduler(L, "level.wait_for_load")->YieldUntil(std::move(predicate));
    return lua_yieldk(L, 0, 0, nullptr);
}

static int WaitForComplete(lua_State *L) {
    RequireScheduler(L, "level.wait_for_complete")->YieldWaitForEvent("level_complete");
    return lua_yieldk(L, 0, 0, nullptr);
}

static int WaitForCheckpoint(lua_State *L) {
    const int targetSector = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushlightuserdata(L, GetContext(L));
    lua_pushinteger(L, targetSector);
    lua_pushcclosure(L, CheckpointPredicate, 2);
    auto predicate = tas::lua::LuaFunction::FromStack(L, -1);
    RequireScheduler(L, "level.wait_for_checkpoint")->YieldUntil(std::move(predicate));
    return lua_yieldk(L, 0, 0, nullptr);
}

void LuaApi::RegisterLevelApi(lua_State *state, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterLevelApi requires a valid ScriptContext");
    }

    tas::lua::LuaStackGuard guard(state);
    lua_getglobal(state, "tas");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_setglobal(state, "tas");
        lua_getglobal(state, "tas");
    }

    lua_newtable(state);
    SetLevelFunction(state, "get_current", GetCurrent, context);
    SetLevelFunction(state, "get_current_number", GetCurrentNumber, context);
    SetLevelFunction(state, "is_loaded", IsLoaded, context);
    SetLevelFunction(state, "is_paused", IsPaused, context);
    SetLevelFunction(state, "get_sector", GetSector, context);
    SetLevelFunction(state, "load", StubLoad, context);
    SetLevelFunction(state, "restart", StubRestart, context);
    SetLevelFunction(state, "exit_to_menu", StubExitToMenu, context);
    SetLevelFunction(state, "next", StubNext, context);
    SetLevelFunction(state, "previous", StubPrevious, context);
    SetLevelFunction(state, "is_completed", IsCompleted, context);
    SetLevelFunction(state, "is_at_checkpoint", IsAtCheckpoint, context);
    SetLevelFunction(state, "wait_for_load", WaitForLoad, context);
    SetLevelFunction(state, "wait_for_complete", WaitForComplete, context);
    SetLevelFunction(state, "wait_for_checkpoint", WaitForCheckpoint, context);
    lua_setfield(state, -2, "level");

    lua_pop(state, 1);
}
