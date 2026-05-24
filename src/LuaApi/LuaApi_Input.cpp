#include "LuaApi.h"

#include "InputSystem.h"
#include "LuaScheduler.h"
#include "ScriptContext.h"

namespace {

ScriptContext *GetContext(lua_State *state) {
    return static_cast<ScriptContext *>(lua_touserdata(state, lua_upvalueindex(1)));
}

InputSystem *RequireInput(lua_State *state) {
    ScriptContext *context = GetContext(state);
    InputSystem *input = context ? context->GetInputSystem() : nullptr;
    if (!input) {
        luaL_error(state, "input system is not available");
    }
    return input;
}

std::string RequireString(lua_State *state, int index, const char *name) {
    if (!lua_isstring(state, index)) {
        luaL_error(state, "%s must be a string", name);
    }
    size_t length = 0;
    const char *value = lua_tolstring(state, index, &length);
    if (!value || length == 0) {
        luaL_error(state, "%s cannot be empty", name);
    }
    return std::string(value, length);
}

int KeyboardPress(lua_State *state) {
    if (lua_gettop(state) != 1) {
        return luaL_error(state, "keyboard.press(key_string): expected one argument");
    }
    RequireInput(state)->PressKeysOneFrame(RequireString(state, 1, "key_string"));
    return 0;
}

int KeyboardKeyDown(lua_State *state) {
    if (lua_gettop(state) != 1) {
        return luaL_error(state, "keyboard.key_down(key_string): expected one argument");
    }
    RequireInput(state)->PressKeys(RequireString(state, 1, "key_string"));
    return 0;
}

int KeyboardHold(lua_State *state) {
    if (lua_gettop(state) != 2 || !lua_isinteger(state, 2)) {
        return luaL_error(state, "keyboard.hold(key_string, duration_ticks): expected string and integer");
    }

    ScriptContext *context = GetContext(state);
    LuaScheduler *scheduler = context ? context->GetScheduler() : nullptr;
    if (!scheduler) {
        return luaL_error(state, "keyboard.hold: scheduler is not available");
    }

    const std::string keyString = RequireString(state, 1, "key_string");
    const int ticks = static_cast<int>(lua_tointeger(state, 2));
    if (ticks <= 0) {
        return luaL_error(state, "keyboard.hold: duration must be positive");
    }

    RequireInput(state)->HoldKeys(keyString, ticks);
    scheduler->YieldTicks(ticks);
    return lua_yieldk(state, 0, 0, nullptr);
}

int KeyboardKeyUp(lua_State *state) {
    if (lua_gettop(state) != 1) {
        return luaL_error(state, "keyboard.key_up(key_string): expected one argument");
    }
    RequireInput(state)->ReleaseKeys(RequireString(state, 1, "key_string"));
    return 0;
}

int KeyboardReleaseAll(lua_State *state) {
    if (lua_gettop(state) != 0) {
        return luaL_error(state, "keyboard.release_all(): expected no arguments");
    }
    RequireInput(state)->ReleaseAllKeys();
    return 0;
}

int KeyboardAreDown(lua_State *state) {
    if (lua_gettop(state) != 1) {
        return luaL_error(state, "keyboard.are_keys_down(key_string): expected one argument");
    }
    lua_pushboolean(state, RequireInput(state)->AreKeysDown(RequireString(state, 1, "key_string")));
    return 1;
}

int KeyboardAreUp(lua_State *state) {
    if (lua_gettop(state) != 1) {
        return luaL_error(state, "keyboard.are_keys_up(key_string): expected one argument");
    }
    lua_pushboolean(state, RequireInput(state)->AreKeysUp(RequireString(state, 1, "key_string")));
    return 1;
}

int KeyboardAvailableKeys(lua_State *state) {
    if (lua_gettop(state) != 0) {
        return luaL_error(state, "keyboard.get_available_keys(): expected no arguments");
    }

    auto keys = RequireInput(state)->GetAvailableKeys();
    lua_createtable(state, static_cast<int>(keys.size()), 0);
    int index = 1;
    for (const auto &key : keys) {
        lua_pushlstring(state, key.data(), key.size());
        lua_rawseti(state, -2, index++);
    }
    return 1;
}

void SetClosure(lua_State *state, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(state, context);
    lua_pushcclosure(state, function, 1);
    lua_setfield(state, -2, name);
}

void Alias(lua_State *state, const char *sourceTable, const char *sourceName, const char *targetName) {
    lua_getglobal(state, "tas");
    lua_getfield(state, -1, sourceTable);
    lua_getfield(state, -1, sourceName);
    lua_setfield(state, -3, targetName);
    lua_pop(state, 2);
}

} // namespace

void LuaApi::RegisterInputApi(lua_State *state, ScriptContext *context) {
    lua_getglobal(state, "tas");
    lua_newtable(state);

    SetClosure(state, "press", KeyboardPress, context);
    SetClosure(state, "hold", KeyboardHold, context);
    SetClosure(state, "key_down", KeyboardKeyDown, context);
    SetClosure(state, "key_up", KeyboardKeyUp, context);
    SetClosure(state, "release_all", KeyboardReleaseAll, context);
    SetClosure(state, "are_keys_down", KeyboardAreDown, context);
    SetClosure(state, "are_keys_up", KeyboardAreUp, context);
    SetClosure(state, "get_available_keys", KeyboardAvailableKeys, context);

    lua_setfield(state, -2, "keyboard");
    lua_pop(state, 1);

    Alias(state, "keyboard", "press", "press");
    Alias(state, "keyboard", "hold", "hold");
    Alias(state, "keyboard", "key_down", "key_down");
    Alias(state, "keyboard", "key_up", "key_up");
    Alias(state, "keyboard", "release_all", "release_all_keys");
    Alias(state, "keyboard", "are_keys_down", "are_keys_down");
    Alias(state, "keyboard", "are_keys_up", "are_keys_up");
}
