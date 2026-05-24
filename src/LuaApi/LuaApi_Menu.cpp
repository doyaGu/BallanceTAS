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
#include <string>

static ScriptContext *GetContext(lua_State *L) {
    return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

static LuaScheduler *RequireScheduler(lua_State *L, const char *functionName) {
    auto *context = GetContext(L);
    auto *scheduler = context ? context->GetScheduler() : nullptr;
    if (!scheduler) {
        luaL_error(L, "%s: Scheduler not available for this context", functionName);
    }
    return scheduler;
}

static void SetMenuFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, function, 1);
    lua_setfield(L, -2, name);
}

static int MenuStubError(lua_State *L, const char *functionName) {
    Log::Warn("[STUB] %s - Not yet implemented", functionName);
    return luaL_error(L, "%s: Not yet implemented - stub function", functionName);
}

static int IsInMenu(lua_State *L) {
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushboolean(L, game && !game->IsPlaying());
    return 1;
}

static int IsInGame(lua_State *L) {
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushboolean(L, game && game->IsPlaying());
    return 1;
}

static int GetCurrent(lua_State *L) {
    return MenuStubError(L, "menu.get_current");
}

static int IsAt(lua_State *L) {
    luaL_checkstring(L, 1);
    return MenuStubError(L, "menu.is_at");
}

static int NavigateTo(lua_State *L) {
    luaL_checkstring(L, 1);
    return MenuStubError(L, "menu.navigate_to");
}

static int ClickButton(lua_State *L) {
    luaL_checkstring(L, 1);
    return MenuStubError(L, "menu.click_button");
}

static int SelectLevel(lua_State *L) {
    luaL_checkstring(L, 1);
    return MenuStubError(L, "menu.select_level");
}

static int GoBack(lua_State *L) {
    return MenuStubError(L, "menu.go_back");
}

static int GoToMain(lua_State *L) {
    return MenuStubError(L, "menu.go_to_main");
}

static int SendKey(lua_State *L) {
    luaL_checkstring(L, 1);
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
        luaL_checkinteger(L, 2);
    }
    return MenuStubError(L, "menu.send_key");
}

static int PressEnter(lua_State *L) {
    return MenuStubError(L, "menu.press_enter");
}

static int PressEscape(lua_State *L) {
    return MenuStubError(L, "menu.press_escape");
}

static int WaitForMenu(lua_State *L) {
    luaL_checkstring(L, 1);
    return MenuStubError(L, "menu.wait_for_menu");
}

static int IsPlayingPredicate(lua_State *L) {
    auto *context = static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
    auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushboolean(L, game && game->IsPlaying());
    return 1;
}

static int IsMenuPredicate(lua_State *L) {
    auto *context = static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
    auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushboolean(L, game && !game->IsPlaying());
    return 1;
}

static int WaitForGameStart(lua_State *L) {
    lua_pushlightuserdata(L, GetContext(L));
    lua_pushcclosure(L, IsPlayingPredicate, 1);
    auto predicate = tas::lua::LuaFunction::FromStack(L, -1);
    RequireScheduler(L, "menu.wait_for_game_start")->YieldUntil(std::move(predicate));
    return lua_yieldk(L, 0, 0, nullptr);
}

static int WaitForMenuEntry(lua_State *L) {
    lua_pushlightuserdata(L, GetContext(L));
    lua_pushcclosure(L, IsMenuPredicate, 1);
    auto predicate = tas::lua::LuaFunction::FromStack(L, -1);
    RequireScheduler(L, "menu.wait_for_menu_entry")->YieldUntil(std::move(predicate));
    return lua_yieldk(L, 0, 0, nullptr);
}

static int GetAvailableLevels(lua_State *L) {
    return MenuStubError(L, "menu.get_available_levels");
}

void LuaApi::RegisterMenuApi(lua_State *state, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterMenuApi requires a valid ScriptContext");
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
    SetMenuFunction(state, "is_in_menu", IsInMenu, context);
    SetMenuFunction(state, "is_in_game", IsInGame, context);
    SetMenuFunction(state, "get_current", GetCurrent, context);
    SetMenuFunction(state, "is_at", IsAt, context);
    SetMenuFunction(state, "navigate_to", NavigateTo, context);
    SetMenuFunction(state, "click_button", ClickButton, context);
    SetMenuFunction(state, "select_level", SelectLevel, context);
    SetMenuFunction(state, "go_back", GoBack, context);
    SetMenuFunction(state, "go_to_main", GoToMain, context);
    SetMenuFunction(state, "send_key", SendKey, context);
    SetMenuFunction(state, "press_enter", PressEnter, context);
    SetMenuFunction(state, "press_escape", PressEscape, context);
    SetMenuFunction(state, "wait_for_menu", WaitForMenu, context);
    SetMenuFunction(state, "wait_for_game_start", WaitForGameStart, context);
    SetMenuFunction(state, "wait_for_menu_entry", WaitForMenuEntry, context);
    SetMenuFunction(state, "get_available_levels", GetAvailableLevels, context);
    lua_setfield(state, -2, "menu");

    lua_pop(state, 1);
}
