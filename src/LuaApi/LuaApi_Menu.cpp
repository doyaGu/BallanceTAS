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

#include <cctype>
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
    // Clicking through menus needs script input to reach the game outside TAS
    // playback, which today only PlaybackService applies.
    return luaL_error(L, "%s: not supported; use tas.level.load()/exit_to_menu() to change screens",
                      functionName);
}

static int PushActionResult(lua_State *L, bool ok, const std::string &error) {
    lua_pushboolean(L, ok);
    if (ok) {
        return 1;
    }
    lua_pushlstring(L, error.data(), error.size());
    return 2;
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

static std::string ActiveMenu(lua_State *L) {
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    return game ? game->GetActiveMenu() : std::string();
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
    const std::string menu = ActiveMenu(L);
    if (menu.empty()) {
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, menu.data(), menu.size());
    }
    return 1;
}

static int IsAt(lua_State *L) {
    size_t length = 0;
    const char *name = luaL_checklstring(L, 1, &length);
    const std::string menu = ActiveMenu(L);
    lua_pushboolean(L, !menu.empty() && menu == NormalizeMenuName(std::string_view(name, length)));
    return 1;
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
    std::optional<int> level;
    if (lua_type(L, 1) == LUA_TNUMBER && lua_isinteger(L, 1) &&
        LevelNames::IsBuiltinLevel(static_cast<int>(lua_tointeger(L, 1)))) {
        level = static_cast<int>(lua_tointeger(L, 1));
    } else if (lua_type(L, 1) == LUA_TSTRING) {
        size_t length = 0;
        const char *text = lua_tolstring(L, 1, &length);
        level = LevelNames::Parse(std::string_view(text, length));
    }
    if (!level) {
        return luaL_argerror(L, 1, "expected a level 1-13 or a name such as \"Level_03\"");
    }
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    std::string error = "game interface is not available";
    const bool ok = game && game->LoadLevel(*level, error);
    return PushActionResult(L, ok, error);
}

static int GoBack(lua_State *L) {
    return MenuStubError(L, "menu.go_back");
}

static int GoToMain(lua_State *L) {
    auto *context = GetContext(L);
    auto *game = context ? context->GetGameInterface() : nullptr;
    if (!game) {
        return PushActionResult(L, false, "game interface is not available");
    }
    if (game->IsIngame()) {
        std::string error;
        const bool ok = game->SendGameFlowEvent(GameInterface::GameFlowEvent::ExitLevel, error);
        return PushActionResult(L, ok, error);
    }
    const std::string menu = game->GetActiveMenu();
    if (menu == "main") {
        return PushActionResult(L, true, {});
    }
    return PushActionResult(L, false, "navigating between menu screens is not supported (current: " +
                                          (menu.empty() ? std::string("none") : menu) + ")");
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

static int MenuAtPredicate(lua_State *L) {
    auto *context = static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
    auto *game = context ? context->GetGameInterface() : nullptr;
    size_t length = 0;
    const char *wanted = lua_tolstring(L, lua_upvalueindex(2), &length);
    const std::string menu = game ? game->GetActiveMenu() : std::string();
    lua_pushboolean(L, !menu.empty() && wanted && menu == std::string(wanted, length));
    return 1;
}

static int WaitForMenu(lua_State *L) {
    size_t length = 0;
    const char *name = luaL_checklstring(L, 1, &length);
    const std::string wanted = NormalizeMenuName(std::string_view(name, length));
    lua_pushlightuserdata(L, GetContext(L));
    lua_pushlstring(L, wanted.data(), wanted.size());
    lua_pushcclosure(L, MenuAtPredicate, 2);
    auto predicate = tas::lua::LuaFunction::FromStack(L, -1);
    RequireScheduler(L, "menu.wait_for_menu")->YieldUntil(std::move(predicate));
    return lua_yieldk(L, 0, 0, nullptr);
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
    // Built-in levels only; whether each is unlocked is not read.
    lua_createtable(L, LevelNames::kLastLevel, 0);
    for (int level = LevelNames::kFirstLevel; level <= LevelNames::kLastLevel; ++level) {
        const std::string key = LevelNames::ToKey(level);
        lua_pushlstring(L, key.data(), key.size());
        lua_rawseti(L, -2, level);
    }
    return 1;
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
