#include "LuaApi.h"

#include "../LuaRuntime/LuaFunction.h"
#include "../LuaRuntime/LuaStackGuard.h"

#include "GameInterface.h"
#include "LevelNames.h"
#include "Logger.h"
#ifdef Yield
#undef Yield
#endif
#include "LuaScheduler.h"
#include "ScriptContext.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

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

// Actions return true, or false plus a reason, instead of raising: whether a
// level can be loaded depends on game state the script may want to handle.
static int PushActionResult(lua_State *L, bool ok, const std::string &error) {
    lua_pushboolean(L, ok);
    if (ok) {
        return 1;
    }
    lua_pushlstring(L, error.data(), error.size());
    return 2;
}

static std::optional<int> CheckLevelArgument(lua_State *L, int index) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        if (!lua_isinteger(L, index)) {
            return std::nullopt;
        }
        const lua_Integer level = lua_tointeger(L, index);
        return LevelNames::IsBuiltinLevel(static_cast<int>(level)) ? std::optional<int>(static_cast<int>(level))
                                                                   : std::nullopt;
    }
    size_t length = 0;
    const char *text = luaL_checklstring(L, index, &length);
    return LevelNames::Parse(std::string_view(text, length));
}

static int LoadLevel(lua_State *L) {
    const std::optional<int> level = CheckLevelArgument(L, 1);
    if (!level) {
        return luaL_argerror(L, 1, "expected a level 1-13 or a name such as \"Level_03\"");
    }
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    std::string error = "game interface is not available";
    const bool ok = game && game->LoadLevel(*level, error);
    return PushActionResult(L, ok, error);
}

static int SendFlowEvent(lua_State *L, GameInterface::GameFlowEvent event) {
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    std::string error = "game interface is not available";
    const bool ok = game && game->SendGameFlowEvent(event, error);
    return PushActionResult(L, ok, error);
}

static int Restart(lua_State *L) {
    return SendFlowEvent(L, GameInterface::GameFlowEvent::ResetLevel);
}

static int ExitToMenu(lua_State *L) {
    return SendFlowEvent(L, GameInterface::GameFlowEvent::ExitLevel);
}

static int Next(lua_State *L) {
    return SendFlowEvent(L, GameInterface::GameFlowEvent::NextLevel);
}

static int IsCompleted(lua_State *L) {
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushboolean(L, game && game->IsLevelCompleted());
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
    RequireScheduler(L, "level.wait_for_complete")->YieldWaitForEvent("level_finish");
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
    SetLevelFunction(state, "load", LoadLevel, context);
    SetLevelFunction(state, "restart", Restart, context);
    SetLevelFunction(state, "exit_to_menu", ExitToMenu, context);
    SetLevelFunction(state, "next", Next, context);
    SetLevelFunction(state, "is_completed", IsCompleted, context);
    SetLevelFunction(state, "is_at_checkpoint", IsAtCheckpoint, context);
    SetLevelFunction(state, "wait_for_load", WaitForLoad, context);
    SetLevelFunction(state, "wait_for_complete", WaitForComplete, context);
    SetLevelFunction(state, "wait_for_checkpoint", WaitForCheckpoint, context);
    lua_setfield(state, -2, "level");

    lua_pop(state, 1);
}
