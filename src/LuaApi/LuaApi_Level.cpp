#include "LuaApi.h"

#include "LuaApiCommon.h"
#include "LuaRuntime/LuaFunction.h"
#include "LuaRuntime/LuaStackGuard.h"

#include "GameInterface.h"
#include "LevelNames.h"
#ifdef Yield
#undef Yield
#endif
#include "LuaScheduler.h"
#include "ScriptContext.h"

#include <optional>
#include <string>
#include <string_view>

using namespace LuaApiCommon;

static GameInterface *RequireGame(lua_State *L, const char *where) {
    ScriptContext *context = GetContext(L);
    GameInterface *game = context ? context->GetGameInterface() : nullptr;
    if (!game) {
        Raise(L, where, "the game interface is not available");
    }
    return game;
}

// tas.level.get_name() -> "Level_03" | nil (no level loaded)
static int GetName(lua_State *L) {
    GameInterface *game = RequireGame(L, "tas.level.get_name");
    if (!game->IsPlaying()) {
        lua_pushnil(L);
        return 1;
    }
    PushString(L, game->GetMapName());
    return 1;
}

// tas.level.get_number() -> 1-13, or 0 when no level is loaded
static int GetNumber(lua_State *L) {
    lua_pushinteger(L, RequireGame(L, "tas.level.get_number")->GetCurrentLevel());
    return 1;
}

static int IsLoaded(lua_State *L) {
    lua_pushboolean(L, RequireGame(L, "tas.level.is_loaded")->IsPlaying());
    return 1;
}

static int IsPaused(lua_State *L) {
    lua_pushboolean(L, RequireGame(L, "tas.level.is_paused")->IsPaused());
    return 1;
}

static int GetSector(lua_State *L) {
    lua_pushinteger(L, RequireGame(L, "tas.level.get_sector")->GetCurrentSector());
    return 1;
}

// A level as 1-13 or a name such as "Level_03"; raises on anything else.
static int CheckLevel(lua_State *L, int arg, const char *where) {
    std::optional<int> level;
    if (lua_type(L, arg) == LUA_TNUMBER) {
        int isInteger = 0;
        const lua_Integer value = lua_tointegerx(L, arg, &isInteger);
        if (isInteger && LevelNames::IsBuiltinLevel(static_cast<int>(value))) {
            level = static_cast<int>(value);
        }
    } else if (lua_type(L, arg) == LUA_TSTRING) {
        size_t length = 0;
        const char *text = lua_tolstring(L, arg, &length);
        level = LevelNames::Parse(std::string_view(text, length));
    }
    if (!level) {
        RaiseArg(L, where, arg, "level 1-13 or a name such as \"Level_03\" expected");
    }
    return *level;
}

// tas.level.load(level) -> true | false, reason
static int LoadLevel(lua_State *L) {
    const int level = CheckLevel(L, 1, "tas.level.load");
    GameInterface *game = RequireGame(L, "tas.level.load");
    std::string reason;
    const bool ok = game->LoadLevel(level, reason);
    return PushResult(L, ok, reason);
}

static int SendFlowEvent(lua_State *L, GameInterface::GameFlowEvent event, const char *where) {
    GameInterface *game = RequireGame(L, where);
    std::string reason;
    const bool ok = game->SendGameFlowEvent(event, reason);
    return PushResult(L, ok, reason);
}

static int Restart(lua_State *L) {
    return SendFlowEvent(L, GameInterface::GameFlowEvent::ResetLevel, "tas.level.restart");
}

static int ExitToMenu(lua_State *L) {
    return SendFlowEvent(L, GameInterface::GameFlowEvent::ExitLevel, "tas.level.exit_to_menu");
}

static int Next(lua_State *L) {
    return SendFlowEvent(L, GameInterface::GameFlowEvent::NextLevel, "tas.level.next");
}

static int IsCompleted(lua_State *L) {
    lua_pushboolean(L, RequireGame(L, "tas.level.is_completed")->IsLevelCompleted());
    return 1;
}

// tas.level.is_at_checkpoint(sector): currently in exactly that sector
static int IsAtCheckpoint(lua_State *L) {
    const lua_Integer sector = luaL_checkinteger(L, 1);
    lua_pushboolean(L, RequireGame(L, "tas.level.is_at_checkpoint")->GetCurrentSector() == sector);
    return 1;
}

static int IsPlayingPredicate(lua_State *L) {
    auto *game = static_cast<GameInterface *>(lua_touserdata(L, lua_upvalueindex(1)));
    lua_pushboolean(L, game && game->IsPlaying());
    return 1;
}

static int CheckpointPredicate(lua_State *L) {
    auto *game = static_cast<GameInterface *>(lua_touserdata(L, lua_upvalueindex(1)));
    const lua_Integer target = lua_tointeger(L, lua_upvalueindex(2));
    lua_pushboolean(L, game && game->GetCurrentSector() >= target);
    return 1;
}

static int WaitForLoad(lua_State *L) {
    LuaScheduler *scheduler = RequireYieldableScheduler(L, "tas.level.wait_for_load");
    lua_pushlightuserdata(L, RequireGame(L, "tas.level.wait_for_load"));
    lua_pushcclosure(L, IsPlayingPredicate, 1);
    scheduler->YieldUntil(tas::lua::LuaFunction::FromStack(L, -1));
    return lua_yieldk(L, 0, 0, nullptr);
}

static int WaitForComplete(lua_State *L) {
    RequireYieldableScheduler(L, "tas.level.wait_for_complete")->YieldWaitForEvent("level_finish");
    return lua_yieldk(L, 0, 0, nullptr);
}

// tas.level.wait_for_checkpoint(sector): until that sector or a later one is reached
static int WaitForCheckpoint(lua_State *L) {
    const lua_Integer target = luaL_checkinteger(L, 1);
    LuaScheduler *scheduler = RequireYieldableScheduler(L, "tas.level.wait_for_checkpoint");
    lua_pushlightuserdata(L, RequireGame(L, "tas.level.wait_for_checkpoint"));
    lua_pushinteger(L, target);
    lua_pushcclosure(L, CheckpointPredicate, 2);
    scheduler->YieldUntil(tas::lua::LuaFunction::FromStack(L, -1));
    return lua_yieldk(L, 0, 0, nullptr);
}

void LuaApi::RegisterLevelApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);
    lua_getglobal(state, "tas");

    lua_newtable(state);
    SetClosure(state, "get_name", GetName, context);
    SetClosure(state, "get_number", GetNumber, context);
    SetClosure(state, "is_loaded", IsLoaded, context);
    SetClosure(state, "is_paused", IsPaused, context);
    SetClosure(state, "get_sector", GetSector, context);
    SetClosure(state, "load", LoadLevel, context);
    SetClosure(state, "restart", Restart, context);
    SetClosure(state, "exit_to_menu", ExitToMenu, context);
    SetClosure(state, "next", Next, context);
    SetClosure(state, "is_completed", IsCompleted, context);
    SetClosure(state, "is_at_checkpoint", IsAtCheckpoint, context);
    SetClosure(state, "wait_for_load", WaitForLoad, context);
    SetClosure(state, "wait_for_complete", WaitForComplete, context);
    SetClosure(state, "wait_for_checkpoint", WaitForCheckpoint, context);
    // Deprecated names, kept as aliases.
    SetAlias(state, "get_current", "get_name");
    SetAlias(state, "get_current_number", "get_number");

    // Top-level shortcuts are the same function objects.
    lua_getfield(state, -1, "get_number");
    lua_setfield(state, -3, "get_level");
    lua_getfield(state, -1, "get_sector");
    lua_setfield(state, -3, "get_sector");
    lua_getfield(state, -1, "is_paused");
    lua_setfield(state, -3, "is_paused");
    lua_getfield(state, -1, "is_loaded");
    lua_setfield(state, -3, "is_playing");

    lua_setfield(state, -2, "level");
    lua_pop(state, 1);
}
