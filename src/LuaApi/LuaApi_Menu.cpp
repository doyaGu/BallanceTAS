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

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

using namespace LuaApiCommon;

static GameInterface *RequireGame(lua_State *L, const char *where) {
    ScriptContext *context = GetContext(L);
    GameInterface *game = context ? context->GetGameInterface() : nullptr;
    if (!game) {
        Raise(L, where, "the game interface is not available");
    }
    return game;
}

/// "Menu_Opt_Keys", "opt_keys" and "options_keys" all name the same screen.
static std::string NormalizeMenuName(std::string_view name) {
    std::string lowered;
    for (char c : name) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (lowered.rfind("menu_", 0) == 0) {
        lowered.erase(0, 5);
    }
    if (lowered.rfind("opt_", 0) == 0) {
        lowered = "options_" + lowered.substr(4);
    }
    return lowered;
}

// Handed to the prelude so navigate_to normalises screen names the same way.
int LuaApi::NormalizeMenuNameFunction(lua_State *L) {
    size_t length = 0;
    const char *name = luaL_checklstring(L, 1, &length);
    PushString(L, NormalizeMenuName(std::string_view(name, length)));
    return 1;
}

static int IsInMenu(lua_State *L) {
    lua_pushboolean(L, !RequireGame(L, "tas.menu.is_in_menu")->IsPlaying());
    return 1;
}

// tas.menu.get_current() -> screen name | nil (no menu shown)
static int GetCurrent(lua_State *L) {
    const std::string menu = RequireGame(L, "tas.menu.get_current")->GetActiveMenu();
    if (menu.empty()) {
        lua_pushnil(L);
    } else {
        PushString(L, menu);
    }
    return 1;
}

static int IsAt(lua_State *L) {
    size_t length = 0;
    const char *name = luaL_checklstring(L, 1, &length);
    GameInterface *game = RequireGame(L, "tas.menu.is_at");
    const std::string menu = game->GetActiveMenu();
    lua_pushboolean(L, !menu.empty() && menu == NormalizeMenuName(std::string_view(name, length)));
    return 1;
}

// tas.menu.click_button(name) -> true | false, reason
static int ClickButton(lua_State *L) {
    size_t length = 0;
    const char *name = CheckNonEmptyString(L, 1, "tas.menu.click_button", &length);
    GameInterface *game = RequireGame(L, "tas.menu.click_button");
    std::string reason;
    const bool ok = game->ClickMenuButton(std::string(name, length), reason);
    return PushResult(L, ok, reason);
}

// tas.menu.go_back() -> true | false, reason
static int GoBack(lua_State *L) {
    GameInterface *game = RequireGame(L, "tas.menu.go_back");
    std::string reason;
    const bool ok = game->ClickMenuButton("back", reason);
    return PushResult(L, ok, reason);
}

// tas.menu.get_buttons([screen]) -> names click_button accepts (current screen by default)
static int GetButtons(lua_State *L) {
    const char *requested = lua_isnoneornil(L, 1) ? nullptr : luaL_checkstring(L, 1);
    GameInterface *game = RequireGame(L, "tas.menu.get_buttons");
    const std::string screen = requested ? NormalizeMenuName(requested) : game->GetActiveMenu();
    PushStringList(L, game->GetMenuButtons(screen));
    return 1;
}

// In game this leaves the level; the prelude wraps it to walk menus otherwise.
static int ExitLevelToMain(lua_State *L) {
    GameInterface *game = RequireGame(L, "tas.menu.go_to_main");
    if (!game->IsIngame()) {
        return game->GetActiveMenu() == "main" ? PushOk(L) : PushFail(L, "not at the main menu");
    }
    std::string reason;
    const bool ok = game->SendGameFlowEvent(GameInterface::GameFlowEvent::ExitLevel, reason);
    return PushResult(L, ok, reason);
}

static int MenuAtPredicate(lua_State *L) {
    auto *game = static_cast<GameInterface *>(lua_touserdata(L, lua_upvalueindex(1)));
    size_t length = 0;
    const char *wanted = lua_tolstring(L, lua_upvalueindex(2), &length);
    const std::string menu = game ? game->GetActiveMenu() : std::string();
    lua_pushboolean(L, !menu.empty() && wanted && menu == std::string_view(wanted, length));
    return 1;
}

static int IsMenuPredicate(lua_State *L) {
    auto *game = static_cast<GameInterface *>(lua_touserdata(L, lua_upvalueindex(1)));
    lua_pushboolean(L, game && !game->IsPlaying());
    return 1;
}

static int WaitForMenu(lua_State *L) {
    size_t length = 0;
    const char *name = CheckNonEmptyString(L, 1, "tas.menu.wait_for_menu", &length);
    LuaScheduler *scheduler = RequireYieldableScheduler(L, "tas.menu.wait_for_menu");
    GameInterface *game = RequireGame(L, "tas.menu.wait_for_menu");
    lua_pushlightuserdata(L, game);
    PushString(L, NormalizeMenuName(std::string_view(name, length)));
    lua_pushcclosure(L, MenuAtPredicate, 2);
    scheduler->YieldUntil(tas::lua::LuaFunction::FromStack(L, -1));
    return lua_yieldk(L, 0, 0, nullptr);
}

static int WaitForMenuEntry(lua_State *L) {
    LuaScheduler *scheduler = RequireYieldableScheduler(L, "tas.menu.wait_for_menu_entry");
    lua_pushlightuserdata(L, RequireGame(L, "tas.menu.wait_for_menu_entry"));
    lua_pushcclosure(L, IsMenuPredicate, 1);
    scheduler->YieldUntil(tas::lua::LuaFunction::FromStack(L, -1));
    return lua_yieldk(L, 0, 0, nullptr);
}

// tas.menu.get_available_levels() -> {"Level_01", ..., "Level_13"} (unlock state is not read)
static int GetAvailableLevels(lua_State *L) {
    lua_createtable(L, LevelNames::kLastLevel, 0);
    for (int level = LevelNames::kFirstLevel; level <= LevelNames::kLastLevel; ++level) {
        PushString(L, LevelNames::ToKey(level));
        lua_rawseti(L, -2, level);
    }
    return 1;
}

void LuaApi::RegisterMenuApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);
    lua_getglobal(state, "tas");

    lua_newtable(state);
    SetClosure(state, "is_in_menu", IsInMenu, context);
    SetClosure(state, "get_current", GetCurrent, context);
    SetClosure(state, "is_at", IsAt, context);
    SetClosure(state, "click_button", ClickButton, context);
    SetClosure(state, "get_buttons", GetButtons, context);
    SetClosure(state, "go_back", GoBack, context);
    SetClosure(state, "go_to_main", ExitLevelToMain, context);
    SetClosure(state, "wait_for_menu", WaitForMenu, context);
    SetClosure(state, "wait_for_menu_entry", WaitForMenuEntry, context);
    SetClosure(state, "get_available_levels", GetAvailableLevels, context);
    // navigate_to, send_key, press_enter, press_escape and go_to_main's menu walk are in the prelude.

    // Same function objects as tas.level.*.
    lua_getfield(state, -2, "level");
    lua_getfield(state, -1, "load");
    lua_setfield(state, -3, "select_level");
    lua_getfield(state, -1, "is_loaded");
    lua_setfield(state, -3, "is_in_game");
    lua_getfield(state, -1, "wait_for_load");
    lua_setfield(state, -3, "wait_for_game_start");
    lua_pop(state, 1);

    lua_setfield(state, -2, "menu");
    lua_pop(state, 1);
}
