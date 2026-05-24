#include "LuaApi.h"

#include "../LuaRuntime/LuaFunction.h"
#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaValue.h"

#include "MessageBus.h"
#include "ScriptContext.h"
#include "ScriptContextManager.h"
#include "SharedDataManager.h"

#include <atomic>

namespace {

std::atomic<uint64_t> g_RequestId{0};

ScriptContext *GetContext(lua_State *state) {
    return static_cast<ScriptContext *>(lua_touserdata(state, lua_upvalueindex(1)));
}

SharedDataManager *GetShared(lua_State *state) {
    ScriptContext *context = GetContext(state);
    ScriptContextManager *manager = context ? context->GetScriptContextManager() : nullptr;
    return manager ? manager->GetSharedDataManager() : nullptr;
}

MessageBus *GetMessageBus(lua_State *state) {
    ScriptContext *context = GetContext(state);
    ScriptContextManager *manager = context ? context->GetScriptContextManager() : nullptr;
    return manager ? manager->GetMessageBus() : nullptr;
}

int ContextName(lua_State *state) {
    ScriptContext *context = GetContext(state);
    if (lua_gettop(state) != 0) {
        return luaL_error(state, "tas.context.get_name(): expected no arguments");
    }
    if (!context) {
        lua_pushliteral(state, "");
        return 1;
    }
    const std::string &name = context->GetName();
    lua_pushlstring(state, name.data(), name.size());
    return 1;
}

int ContextType(lua_State *state) {
    ScriptContext *context = GetContext(state);
    if (lua_gettop(state) != 0) {
        return luaL_error(state, "tas.context.get_type(): expected no arguments");
    }
    lua_pushinteger(state, context ? static_cast<lua_Integer>(context->GetType()) : 0);
    return 1;
}

std::string CheckKey(lua_State *state, int index, const char *functionName) {
    size_t length = 0;
    const char *key = luaL_checklstring(state, index, &length);
    if (!key || length == 0) {
        luaL_error(state, "%s: key cannot be empty", functionName);
    }
    return std::string(key, length);
}

tas::lua::LuaValue CheckPortableValue(lua_State *state, int index, const char *functionName) {
    auto value = tas::lua::LuaValue::FromStack(state, index);
    if (value.IsError()) {
        luaL_error(state, "%s: %s", functionName, value.GetError().message.c_str());
    }
    return value.Unwrap();
}

int SharedSet(lua_State *state) {
    const int argc = lua_gettop(state);
    if (argc != 2 && argc != 3) {
        return luaL_error(state, "tas.shared.set(key, value[, ttl_ms]): expected 2 or 3 arguments");
    }
    SharedDataManager *shared = GetShared(state);
    if (!shared) {
        return luaL_error(state, "tas.shared.set(): shared data manager is unavailable");
    }

    const std::string key = SharedDataManager::MakeSharedKey(CheckKey(state, 1, "tas.shared.set"));
    tas::lua::LuaValue value = CheckPortableValue(state, 2, "tas.shared.set");
    const int ttl = argc == 3 ? static_cast<int>(luaL_checkinteger(state, 3)) : 0;
    lua_pushboolean(state, shared->Set(key, value, SharedDataManager::SetOptions(ttl)) ? 1 : 0);
    return 1;
}

int SharedGet(lua_State *state) {
    const int argc = lua_gettop(state);
    if (argc != 1 && argc != 2) {
        return luaL_error(state, "tas.shared.get(key[, default]): expected 1 or 2 arguments");
    }
    SharedDataManager *shared = GetShared(state);
    if (!shared) {
        return luaL_error(state, "tas.shared.get(): shared data manager is unavailable");
    }

    const std::string key = CheckKey(state, 1, "tas.shared.get");
    tas::lua::LuaValue fallback;
    if (argc == 2) {
        fallback = CheckPortableValue(state, 2, "tas.shared.get");
    }
    shared->Get(SharedDataManager::MakeSharedKey(key), fallback).Push(state);
    return 1;
}

int SharedHas(lua_State *state) {
    if (lua_gettop(state) != 1) {
        return luaL_error(state, "tas.shared.has(key): expected key");
    }
    SharedDataManager *shared = GetShared(state);
    lua_pushboolean(state, shared && shared->Has(SharedDataManager::MakeSharedKey(CheckKey(state, 1, "tas.shared.has"))) ? 1 : 0);
    return 1;
}

int SharedRemove(lua_State *state) {
    if (lua_gettop(state) != 1) {
        return luaL_error(state, "tas.shared.remove(key): expected key");
    }
    SharedDataManager *shared = GetShared(state);
    if (!shared) {
        return luaL_error(state, "tas.shared.remove(): shared data manager is unavailable");
    }
    lua_pushboolean(state, shared->Remove(SharedDataManager::MakeSharedKey(CheckKey(state, 1, "tas.shared.remove"))) ? 1 : 0);
    return 1;
}

int SharedClear(lua_State *state) {
    if (lua_gettop(state) != 0) {
        return luaL_error(state, "tas.shared.clear(): expected no arguments");
    }
    SharedDataManager *shared = GetShared(state);
    if (!shared) {
        return luaL_error(state, "tas.shared.clear(): shared data manager is unavailable");
    }
    shared->ClearNamespace("shared:");
    return 0;
}

int SharedKeys(lua_State *state) {
    if (lua_gettop(state) != 0) {
        return luaL_error(state, "tas.shared.keys(): expected no arguments");
    }
    SharedDataManager *shared = GetShared(state);
    lua_newtable(state);
    if (!shared) {
        return 1;
    }

    int index = 1;
    for (const std::string &key : shared->GetKeys()) {
        static constexpr const char *prefix = "shared:";
        if (key.rfind(prefix, 0) != 0) {
            continue;
        }
        const std::string userKey = key.substr(7);
        lua_pushlstring(state, userKey.data(), userKey.size());
        lua_seti(state, -2, index++);
    }
    return 1;
}

int SharedWatch(lua_State *state) {
    if (lua_gettop(state) != 2 || !lua_isfunction(state, 2)) {
        return luaL_error(state, "tas.shared.watch(key, fn): expected key and function");
    }
    ScriptContext *context = GetContext(state);
    SharedDataManager *shared = GetShared(state);
    if (!context || !shared) {
        return luaL_error(state, "tas.shared.watch(): context shared data manager is unavailable");
    }

    const std::string key = CheckKey(state, 1, "tas.shared.watch");
    lua_pushvalue(state, 2);
    lua_pushlstring(state, key.data(), key.size());
    lua_pushcclosure(state, [](lua_State *L) -> int {
        if (lua_gettop(L) < 2) {
            return luaL_error(L, "tas.shared.watch dispatch: expected new and old values");
        }
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, lua_upvalueindex(2));
        lua_call(L, 3, 0);
        return 0;
    }, 2);
    shared->Watch(context->GetName(), std::weak_ptr<ScriptContext>(),
                  SharedDataManager::MakeSharedKey(key), tas::lua::LuaFunction::FromStack(state, -1));
    lua_pop(state, 1);
    lua_pushboolean(state, 1);
    return 1;
}

int SharedUnwatch(lua_State *state) {
    if (lua_gettop(state) != 1) {
        return luaL_error(state, "tas.shared.unwatch(key): expected key");
    }
    ScriptContext *context = GetContext(state);
    SharedDataManager *shared = GetShared(state);
    if (!context || !shared) {
        return luaL_error(state, "tas.shared.unwatch(): context shared data manager is unavailable");
    }
    shared->Unwatch(context->GetName(), SharedDataManager::MakeSharedKey(CheckKey(state, 1, "tas.shared.unwatch")));
    lua_pushboolean(state, 1);
    return 1;
}

int MessageSubscribe(lua_State *state) {
    if (lua_gettop(state) != 2 || !lua_isfunction(state, 2)) {
        return luaL_error(state, "tas.message.subscribe(type, fn): expected type and function");
    }
    ScriptContext *context = GetContext(state);
    MessageBus *bus = GetMessageBus(state);
    if (!context || !bus) {
        return luaL_error(state, "tas.message.subscribe(): message bus is unavailable");
    }

    const std::string type = CheckKey(state, 1, "tas.message.subscribe");
    bus->RegisterLuaHandler(context->GetName(), std::weak_ptr<ScriptContext>(), type, tas::lua::LuaFunction::FromStack(state, 2));
    lua_pushboolean(state, 1);
    return 1;
}

int MessageUnsubscribe(lua_State *state) {
    if (lua_gettop(state) != 1) {
        return luaL_error(state, "tas.message.unsubscribe(type): expected type");
    }
    ScriptContext *context = GetContext(state);
    MessageBus *bus = GetMessageBus(state);
    if (!context || !bus) {
        return luaL_error(state, "tas.message.unsubscribe(): message bus is unavailable");
    }
    bus->RemoveHandler(context->GetName(), CheckKey(state, 1, "tas.message.unsubscribe"));
    lua_pushboolean(state, 1);
    return 1;
}

int MessageSend(lua_State *state) {
    const int argc = lua_gettop(state);
    if (argc != 3) {
        return luaL_error(state, "tas.message.send(target, type, payload): expected 3 arguments");
    }
    ScriptContext *context = GetContext(state);
    MessageBus *bus = GetMessageBus(state);
    if (!context || !bus) {
        return luaL_error(state, "tas.message.send(): message bus is unavailable");
    }

    const std::string target = CheckKey(state, 1, "tas.message.send");
    const std::string type = CheckKey(state, 2, "tas.message.send");
    tas::lua::LuaValue data = CheckPortableValue(state, 3, "tas.message.send");
    const bool ok = bus->SendMessage(context->GetName(), target, type, data);
    lua_pushboolean(state, ok ? 1 : 0);
    return 1;
}

int MessageBroadcast(lua_State *state) {
    if (lua_gettop(state) != 2) {
        return luaL_error(state, "tas.message.broadcast(type, data): expected type and data");
    }
    ScriptContext *context = GetContext(state);
    MessageBus *bus = GetMessageBus(state);
    if (!context || !bus) {
        return luaL_error(state, "tas.message.broadcast(): message bus is unavailable");
    }
    const std::string type = CheckKey(state, 1, "tas.message.broadcast");
    tas::lua::LuaValue data = CheckPortableValue(state, 2, "tas.message.broadcast");
    lua_pushboolean(state, bus->BroadcastMessage(context->GetName(), type, data) ? 1 : 0);
    return 1;
}

int MessageRespond(lua_State *state) {
    if (lua_gettop(state) != 3) {
        return luaL_error(state, "tas.message.respond(target, correlation_id, data): expected 3 arguments");
    }
    ScriptContext *context = GetContext(state);
    MessageBus *bus = GetMessageBus(state);
    if (!context || !bus) {
        return luaL_error(state, "tas.message.respond(): message bus is unavailable");
    }
    const std::string target = CheckKey(state, 1, "tas.message.respond");
    const std::string correlation = CheckKey(state, 2, "tas.message.respond");
    tas::lua::LuaValue data = CheckPortableValue(state, 3, "tas.message.respond");
    lua_pushboolean(state, bus->SendResponse(context->GetName(), target, correlation, data) ? 1 : 0);
    return 1;
}

int MessageRequestAsync(lua_State *state) {
    if (lua_gettop(state) != 3) {
        return luaL_error(state, "tas.message.request_async(target, type, data): expected 3 arguments");
    }
    ScriptContext *context = GetContext(state);
    MessageBus *bus = GetMessageBus(state);
    if (!context || !bus) {
        return luaL_error(state, "tas.message.request_async(): message bus is unavailable");
    }
    const std::string target = CheckKey(state, 1, "tas.message.request_async");
    const std::string type = CheckKey(state, 2, "tas.message.request_async");
    tas::lua::LuaValue data = CheckPortableValue(state, 3, "tas.message.request_async");
    const std::string correlation = context->GetName() + ":req_" + std::to_string(g_RequestId.fetch_add(1));
    if (!bus->SendRequestAsync(context->GetName(), target, type, data, correlation)) {
        lua_pushnil(state);
        return 1;
    }
    lua_pushlstring(state, correlation.data(), correlation.size());
    return 1;
}

void SetClosure(lua_State *state, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(state, context);
    lua_pushcclosure(state, function, 1);
    lua_setfield(state, -2, name);
}

void RegisterSharedTable(lua_State *state, ScriptContext *context) {
    lua_newtable(state);
    SetClosure(state, "set", SharedSet, context);
    SetClosure(state, "get", SharedGet, context);
    SetClosure(state, "has", SharedHas, context);
    SetClosure(state, "remove", SharedRemove, context);
    SetClosure(state, "clear", SharedClear, context);
    SetClosure(state, "keys", SharedKeys, context);
    SetClosure(state, "watch", SharedWatch, context);
    SetClosure(state, "unwatch", SharedUnwatch, context);
}

void RegisterMessageTable(lua_State *state, ScriptContext *context) {
    lua_newtable(state);
    SetClosure(state, "send", MessageSend, context);
    SetClosure(state, "broadcast", MessageBroadcast, context);
    SetClosure(state, "subscribe", MessageSubscribe, context);
    SetClosure(state, "unsubscribe", MessageUnsubscribe, context);
    SetClosure(state, "respond", MessageRespond, context);
    SetClosure(state, "request_async", MessageRequestAsync, context);
}

} // namespace

void LuaApi::RegisterContextCommunicationApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);
    lua_getglobal(state, "tas");

    lua_newtable(state);
    SetClosure(state, "get_name", ContextName, context);
    SetClosure(state, "get_type", ContextType, context);
    lua_setfield(state, -2, "context");

    RegisterSharedTable(state, context);
    lua_setfield(state, -2, "shared");

    RegisterMessageTable(state, context);
    lua_setfield(state, -2, "message");

    lua_pop(state, 1);
}
