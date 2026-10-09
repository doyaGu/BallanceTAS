#include "LuaApi.h"

#include "InputSystem.h"
#include "LuaApiCommon.h"
#ifdef Yield
#undef Yield
#endif
#include "LuaScheduler.h"
#include "ScriptContext.h"

using namespace LuaApiCommon;

// Key strings name one or more keys: "up", "up right", "shift,left".

static InputSystem *RequireInput(lua_State *L, const char *where) {
    ScriptContext *context = GetContext(L);
    InputSystem *input = context ? context->GetInputSystem() : nullptr;
    if (!input) {
        Raise(L, where, "the input system is not available");
    }
    return input;
}

static int KeyboardPress(lua_State *L) {
    const char *keys = CheckNonEmptyString(L, 1, "tas.keyboard.press");
    RequireInput(L, "tas.keyboard.press")->PressKeysOneFrame(keys);
    return 0;
}

static int KeyboardKeyDown(lua_State *L) {
    const char *keys = CheckNonEmptyString(L, 1, "tas.keyboard.key_down");
    RequireInput(L, "tas.keyboard.key_down")->PressKeys(keys);
    return 0;
}

// tas.keyboard.hold(keys, ticks): holds the keys and waits until they are released
static int KeyboardHold(lua_State *L) {
    const char *keys = CheckNonEmptyString(L, 1, "tas.keyboard.hold");
    const int ticks = CheckTicks(L, 2, "tas.keyboard.hold");
    LuaScheduler *scheduler = RequireYieldableScheduler(L, "tas.keyboard.hold");
    RequireInput(L, "tas.keyboard.hold")->HoldKeys(keys, ticks);
    scheduler->YieldTicks(ticks);
    return lua_yieldk(L, 0, 0, nullptr);
}

static int KeyboardKeyUp(lua_State *L) {
    const char *keys = CheckNonEmptyString(L, 1, "tas.keyboard.key_up");
    RequireInput(L, "tas.keyboard.key_up")->ReleaseKeys(keys);
    return 0;
}

static int KeyboardReleaseAll(lua_State *L) {
    RequireInput(L, "tas.keyboard.release_all")->ReleaseAllKeys();
    return 0;
}

static int KeyboardAreDown(lua_State *L) {
    const char *keys = CheckNonEmptyString(L, 1, "tas.keyboard.are_keys_down");
    lua_pushboolean(L, RequireInput(L, "tas.keyboard.are_keys_down")->AreKeysDown(keys));
    return 1;
}

static int KeyboardAreUp(lua_State *L) {
    const char *keys = CheckNonEmptyString(L, 1, "tas.keyboard.are_keys_up");
    lua_pushboolean(L, RequireInput(L, "tas.keyboard.are_keys_up")->AreKeysUp(keys));
    return 1;
}

static int KeyboardAvailableKeys(lua_State *L) {
    PushStringList(L, RequireInput(L, "tas.keyboard.get_available_keys")->GetAvailableKeys());
    return 1;
}

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

    // tas.press, tas.hold, ... are the same function objects as tas.keyboard.*.
    for (const char *name : {"press", "hold", "key_down", "key_up", "are_keys_down", "are_keys_up"}) {
        lua_getfield(state, -1, name);
        lua_setfield(state, -3, name);
    }
    lua_getfield(state, -1, "release_all");
    lua_setfield(state, -3, "release_all_keys");

    lua_setfield(state, -2, "keyboard");
    lua_pop(state, 1);
}
