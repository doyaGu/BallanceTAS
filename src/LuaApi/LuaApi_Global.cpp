#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaValue.h"

#include "ScriptContext.h"
#include "ScriptContextManager.h"
#include "SharedDataManager.h"

#include <stdexcept>

static ScriptContext *GetContext(lua_State *L) {
    return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

static SharedDataManager *GetShared(lua_State *L) {
    ScriptContext *context = GetContext(L);
    ScriptContextManager *manager = context ? context->GetScriptContextManager() : nullptr;
    return manager ? manager->GetSharedDataManager() : nullptr;
}

static void SetGlobalFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, function, 1);
    lua_setfield(L, -2, name);
}

static const char *CheckKey(lua_State *L, int index, const char *functionName, size_t *length) {
    const char *key = luaL_checklstring(L, index, length);
    if (!key || *length == 0) {
        luaL_error(L, "%s: key cannot be empty", functionName);
    }
    return key;
}

static tas::lua::LuaValue CheckPortableValue(lua_State *L, int index, const char *functionName) {
    auto value = tas::lua::LuaValue::FromStack(L, index);
    if (value.IsError()) {
        luaL_error(L, "%s: %s", functionName, value.GetError().message.c_str());
    }
    return value.Unwrap();
}

static int SetData(lua_State *L) {
    if (lua_gettop(L) != 2) {
        return luaL_error(L, "tas.global.set(key, value): expected key and value");
    }

    size_t keyLength = 0;
    const char *key = CheckKey(L, 1, "tas.global.set", &keyLength);
    SharedDataManager *shared = GetShared(L);
    if (!shared) {
        return luaL_error(L, "tas.global.set(): shared data manager is unavailable");
    }

    shared->Set(SharedDataManager::MakeGlobalKey(std::string(key, keyLength)), CheckPortableValue(L, 2, "global.set"));
    return 0;
}

static int GetData(lua_State *L) {
    if (lua_gettop(L) != 1) {
        return luaL_error(L, "tas.global.get(key): expected key");
    }

    size_t keyLength = 0;
    const char *key = CheckKey(L, 1, "tas.global.get", &keyLength);
    SharedDataManager *shared = GetShared(L);
    if (!shared) {
        return luaL_error(L, "tas.global.get(): shared data manager is unavailable");
    }

    shared->Get(SharedDataManager::MakeGlobalKey(std::string(key, keyLength))).Push(L);
    return 1;
}

static int HasData(lua_State *L) {
    if (lua_gettop(L) != 1 || !lua_isstring(L, 1)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    size_t keyLength = 0;
    const char *key = lua_tolstring(L, 1, &keyLength);
    SharedDataManager *shared = GetShared(L);
    if (!shared || !key || keyLength == 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, shared->Has(SharedDataManager::MakeGlobalKey(std::string(key, keyLength))) ? 1 : 0);
    return 1;
}

static int ClearData(lua_State *L) {
    if (lua_gettop(L) != 1) {
        return luaL_error(L, "tas.global.remove(key): expected key");
    }

    size_t keyLength = 0;
    const char *key = CheckKey(L, 1, "tas.global.remove", &keyLength);
    SharedDataManager *shared = GetShared(L);
    if (!shared) {
        return luaL_error(L, "tas.global.remove(): shared data manager is unavailable");
    }
    shared->Remove(SharedDataManager::MakeGlobalKey(std::string(key, keyLength)));
    return 0;
}

static int ClearAllData(lua_State *L) {
    if (lua_gettop(L) != 0) {
        return luaL_error(L, "tas.global.clear(): expected no arguments");
    }
    SharedDataManager *shared = GetShared(L);
    if (!shared) {
        return luaL_error(L, "tas.global.clear(): shared data manager is unavailable");
    }
    shared->ClearNamespace("global:");
    return 0;
}

static int GetAllKeys(lua_State *L) {
    lua_newtable(L);
    SharedDataManager *shared = GetShared(L);
    if (!shared) {
        return 1;
    }

    int index = 1;
    for (const auto &key : shared->GetKeys()) {
        static constexpr const char *prefix = "global:";
        if (key.rfind(prefix, 0) != 0) {
            continue;
        }
        const std::string userKey = key.substr(7);
        lua_pushlstring(L, userKey.data(), userKey.size());
        lua_seti(L, -2, index++);
    }
    return 1;
}

void LuaApi::RegisterGlobalApi(lua_State *state, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterGlobalApi requires a valid ScriptContext");
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
    SetGlobalFunction(state, "set", SetData, context);
    SetGlobalFunction(state, "get", GetData, context);
    SetGlobalFunction(state, "has", HasData, context);
    SetGlobalFunction(state, "remove", ClearData, context);
    SetGlobalFunction(state, "clear", ClearAllData, context);
    SetGlobalFunction(state, "keys", GetAllKeys, context);
    lua_setfield(state, -2, "global");

    lua_pop(state, 1);
}
