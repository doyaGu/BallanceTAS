#include "LuaApi.h"

#include "LuaApiCommon.h"
#include "LuaRuntime/LuaFunction.h"
#include "LuaRuntime/LuaStackGuard.h"
#include "LuaRuntime/LuaValue.h"

#include "MessageBus.h"
#include "ScriptContext.h"
#include "ScriptContextManager.h"
#include "SharedDataManager.h"

#include <atomic>
#include <cstdio>
#include <string>

using namespace LuaApiCommon;

static std::atomic<uint64_t> g_RequestId{0};

static ScriptContextManager *ContextManager(lua_State *L) {
    ScriptContext *context = GetContext(L);
    return context ? context->GetScriptContextManager() : nullptr;
}

static SharedDataManager *RequireShared(lua_State *L, const char *where) {
    ScriptContextManager *manager = ContextManager(L);
    SharedDataManager *shared = manager ? manager->GetSharedDataManager() : nullptr;
    if (!shared) {
        Raise(L, where, "the shared data manager is not available");
    }
    return shared;
}

static MessageBus *RequireMessageBus(lua_State *L, const char *where) {
    ScriptContextManager *manager = ContextManager(L);
    MessageBus *bus = manager ? manager->GetMessageBus() : nullptr;
    if (!bus) {
        Raise(L, where, "the message bus is not available");
    }
    return bus;
}

// Raises unless the value at `arg` can cross VMs (no functions, userdata, cycles).
// Converting is done afterwards by PortableValue, once nothing can raise any more.
static void CheckPortable(lua_State *L, int arg, const char *where) {
    bool portable = true;
    {
        // Scoped so the Result is destroyed before raising (no destructors run past a raise).
        auto value = tas::lua::LuaValue::FromStack(L, arg);
        if (value.IsError()) {
            portable = false;
            lua_pushstring(L, value.GetError().message.c_str());
        }
    }
    if (!portable) {
        Raise(L, where, "bad argument #%d (%s)", arg, lua_tostring(L, -1));
    }
}

static tas::lua::LuaValue PortableValue(lua_State *L, int arg) {
    auto value = tas::lua::LuaValue::FromStack(L, arg);
    return value.IsOk() ? value.Unwrap() : tas::lua::LuaValue();
}

// ---- tas.context ----------------------------------------------------------------------

static int ContextName(lua_State *L) {
    ScriptContext *context = GetContext(L);
    PushString(L, context ? context->GetName() : std::string());
    return 1;
}

static int ContextType(lua_State *L) {
    ScriptContext *context = GetContext(L);
    lua_pushinteger(L, context ? static_cast<lua_Integer>(context->GetType()) : 0);
    return 1;
}

// ---- tas.shared / tas.global: one store, two namespaces ---------------------------------
// Closures carry the context (upvalue 1) and the namespace prefix (upvalue 2).

struct StoreNamespace {
    const char *module; // "tas.shared"
    const char *prefix; // "shared:"
};

static const StoreNamespace kSharedNamespace{"tas.shared", "shared:"};
static const StoreNamespace kGlobalNamespace{"tas.global", "global:"};

static const StoreNamespace &Namespace(lua_State *L) {
    return *static_cast<const StoreNamespace *>(lua_touserdata(L, lua_upvalueindex(2)));
}

static std::string StoreKey(const StoreNamespace &ns, const char *key, size_t length) {
    return ns.prefix + std::string(key, length);
}

// "tas.shared.set" style names for messages. Kept off the Lua stack so argument
// slots stay as the caller passed them, and in a plain buffer so raising is safe.
struct StoreWhere {
    char text[32];
};

static StoreWhere MakeWhere(const StoreNamespace &ns, const char *fn) {
    StoreWhere where;
    std::snprintf(where.text, sizeof(where.text), "%s.%s", ns.module, fn);
    return where;
}

// store.set(key, value[, ttl_ms]) -> true | false, reason
static int StoreSet(lua_State *L) {
    const StoreNamespace &ns = Namespace(L);
    const StoreWhere storeWhere = MakeWhere(ns, "set");
    const char *where = storeWhere.text;
    size_t length = 0;
    const char *key = CheckNonEmptyString(L, 1, where, &length);
    luaL_checkany(L, 2);
    const int ttl = lua_isnoneornil(L, 3) ? 0 : static_cast<int>(luaL_checkinteger(L, 3));
    CheckPortable(L, 2, where);
    SharedDataManager *shared = RequireShared(L, where);
    const bool ok = shared->Set(StoreKey(ns, key, length), PortableValue(L, 2), SharedDataManager::SetOptions(ttl));
    return PushResult(L, ok, "the value could not be stored (see the log)");
}

// store.get(key[, default]) -> value | default | nil
static int StoreGet(lua_State *L) {
    const StoreNamespace &ns = Namespace(L);
    const StoreWhere storeWhere = MakeWhere(ns, "get");
    const char *where = storeWhere.text;
    size_t length = 0;
    const char *key = CheckNonEmptyString(L, 1, where, &length);
    if (!lua_isnoneornil(L, 2)) {
        CheckPortable(L, 2, where);
    }
    SharedDataManager *shared = RequireShared(L, where);
    shared->Get(StoreKey(ns, key, length), lua_isnoneornil(L, 2) ? tas::lua::LuaValue() : PortableValue(L, 2)).Push(L);
    return 1;
}

static int StoreHas(lua_State *L) {
    const StoreNamespace &ns = Namespace(L);
    const StoreWhere storeWhere = MakeWhere(ns, "has");
    const char *where = storeWhere.text;
    size_t length = 0;
    const char *key = CheckNonEmptyString(L, 1, where, &length);
    SharedDataManager *shared = RequireShared(L, where);
    lua_pushboolean(L, shared->Has(StoreKey(ns, key, length)));
    return 1;
}

// store.remove(key) -> true | false, reason
static int StoreRemove(lua_State *L) {
    const StoreNamespace &ns = Namespace(L);
    const StoreWhere storeWhere = MakeWhere(ns, "remove");
    const char *where = storeWhere.text;
    size_t length = 0;
    const char *key = CheckNonEmptyString(L, 1, where, &length);
    SharedDataManager *shared = RequireShared(L, where);
    const bool ok = shared->Remove(StoreKey(ns, key, length));
    return PushResult(L, ok, "no value is stored under that key");
}

static int StoreClear(lua_State *L) {
    const StoreNamespace &ns = Namespace(L);
    RequireShared(L, MakeWhere(ns, "clear").text)->ClearNamespace(ns.prefix);
    return 0;
}

static int StoreKeys(lua_State *L) {
    const StoreNamespace &ns = Namespace(L);
    SharedDataManager *shared = RequireShared(L, MakeWhere(ns, "keys").text);
    const size_t prefixLength = std::char_traits<char>::length(ns.prefix);
    lua_newtable(L);
    lua_Integer index = 1;
    for (const std::string &key : shared->GetKeys()) {
        if (key.compare(0, prefixLength, ns.prefix) == 0) {
            lua_pushlstring(L, key.data() + prefixLength, key.size() - prefixLength);
            lua_rawseti(L, -2, index++);
        }
    }
    return 1;
}

static int WatchDispatch(lua_State *L) {
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, lua_upvalueindex(2));
    lua_call(L, 3, 0);
    return 0;
}

// tas.shared.watch(key, fn(new, old, key))
static int SharedWatch(lua_State *L) {
    size_t length = 0;
    const char *key = CheckNonEmptyString(L, 1, "tas.shared.watch", &length);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    SharedDataManager *shared = RequireShared(L, "tas.shared.watch");
    ScriptContext *context = GetContext(L);
    lua_pushvalue(L, 2);
    lua_pushlstring(L, key, length);
    lua_pushcclosure(L, WatchDispatch, 2);
    shared->Watch(context->GetName(), std::weak_ptr<ScriptContext>(), StoreKey(kSharedNamespace, key, length),
                  tas::lua::LuaFunction::FromStack(L, -1));
    lua_pop(L, 1);
    return 0;
}

static int SharedUnwatch(lua_State *L) {
    size_t length = 0;
    const char *key = CheckNonEmptyString(L, 1, "tas.shared.unwatch", &length);
    SharedDataManager *shared = RequireShared(L, "tas.shared.unwatch");
    shared->Unwatch(GetContext(L)->GetName(), StoreKey(kSharedNamespace, key, length));
    return 0;
}

// ---- tas.message ------------------------------------------------------------------------

// tas.message.subscribe(type, fn(data, message))
static int MessageSubscribe(lua_State *L) {
    const char *type = CheckNonEmptyString(L, 1, "tas.message.subscribe");
    luaL_checktype(L, 2, LUA_TFUNCTION);
    MessageBus *bus = RequireMessageBus(L, "tas.message.subscribe");
    bus->RegisterLuaHandler(GetContext(L)->GetName(), std::weak_ptr<ScriptContext>(), type,
                            tas::lua::LuaFunction::FromStack(L, 2));
    return 0;
}

static int MessageUnsubscribe(lua_State *L) {
    const char *type = CheckNonEmptyString(L, 1, "tas.message.unsubscribe");
    RequireMessageBus(L, "tas.message.unsubscribe")->RemoveHandler(GetContext(L)->GetName(), type);
    return 0;
}

// tas.message.send(target, type, data) -> true | false, reason
static int MessageSend(lua_State *L) {
    const char *target = CheckNonEmptyString(L, 1, "tas.message.send");
    const char *type = CheckNonEmptyString(L, 2, "tas.message.send");
    CheckPortable(L, 3, "tas.message.send");
    MessageBus *bus = RequireMessageBus(L, "tas.message.send");
    const bool ok = bus->SendMessage(GetContext(L)->GetName(), target, type, PortableValue(L, 3));
    return PushResult(L, ok, "the message was not queued (unknown target or full queue)");
}

// tas.message.broadcast(type, data) -> true | false, reason
static int MessageBroadcast(lua_State *L) {
    const char *type = CheckNonEmptyString(L, 1, "tas.message.broadcast");
    CheckPortable(L, 2, "tas.message.broadcast");
    MessageBus *bus = RequireMessageBus(L, "tas.message.broadcast");
    const bool ok = bus->BroadcastMessage(GetContext(L)->GetName(), type, PortableValue(L, 2));
    return PushResult(L, ok, "the message was not queued (full queue)");
}

// tas.message.respond(target, correlation_id, data) -> true | false, reason
static int MessageRespond(lua_State *L) {
    const char *target = CheckNonEmptyString(L, 1, "tas.message.respond");
    const char *correlation = CheckNonEmptyString(L, 2, "tas.message.respond");
    CheckPortable(L, 3, "tas.message.respond");
    MessageBus *bus = RequireMessageBus(L, "tas.message.respond");
    const bool ok = bus->SendResponse(GetContext(L)->GetName(), target, correlation, PortableValue(L, 3));
    return PushResult(L, ok, "the response was not queued (unknown target or full queue)");
}

// tas.message.request_async(target, type, data) -> correlation_id | nil, reason
static int MessageRequestAsync(lua_State *L) {
    const char *target = CheckNonEmptyString(L, 1, "tas.message.request_async");
    const char *type = CheckNonEmptyString(L, 2, "tas.message.request_async");
    CheckPortable(L, 3, "tas.message.request_async");
    MessageBus *bus = RequireMessageBus(L, "tas.message.request_async");
    ScriptContext *context = GetContext(L);
    const std::string correlation = context->GetName() + ":req_" + std::to_string(g_RequestId.fetch_add(1));
    if (!bus->SendRequestAsync(context->GetName(), target, type, PortableValue(L, 3), correlation)) {
        return PushNilReason(L, "the request was not queued (unknown target or full queue)");
    }
    PushString(L, correlation);
    return 1;
}

// ---- registration -----------------------------------------------------------------------

static void SetStoreFunction(lua_State *L, const char *name, lua_CFunction fn, ScriptContext *context,
                             const StoreNamespace &ns) {
    lua_pushlightuserdata(L, context);
    lua_pushlightuserdata(L, const_cast<StoreNamespace *>(&ns));
    lua_pushcclosure(L, fn, 2);
    lua_setfield(L, -2, name);
}

static void RegisterStore(lua_State *L, ScriptContext *context, const StoreNamespace &ns) {
    lua_newtable(L);
    SetStoreFunction(L, "set", StoreSet, context, ns);
    SetStoreFunction(L, "get", StoreGet, context, ns);
    SetStoreFunction(L, "has", StoreHas, context, ns);
    SetStoreFunction(L, "remove", StoreRemove, context, ns);
    SetStoreFunction(L, "clear", StoreClear, context, ns);
    SetStoreFunction(L, "keys", StoreKeys, context, ns);
}

void LuaApi::RegisterContextCommunicationApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);
    lua_getglobal(state, "tas");

    lua_newtable(state);
    SetClosure(state, "get_name", ContextName, context);
    SetClosure(state, "get_type", ContextType, context);
    lua_setfield(state, -2, "context");

    RegisterStore(state, context, kSharedNamespace);
    SetClosure(state, "watch", SharedWatch, context);
    SetClosure(state, "unwatch", SharedUnwatch, context);
    lua_setfield(state, -2, "shared");

    lua_newtable(state);
    SetClosure(state, "send", MessageSend, context);
    SetClosure(state, "broadcast", MessageBroadcast, context);
    SetClosure(state, "subscribe", MessageSubscribe, context);
    SetClosure(state, "unsubscribe", MessageUnsubscribe, context);
    SetClosure(state, "respond", MessageRespond, context);
    SetClosure(state, "request_async", MessageRequestAsync, context);
    lua_setfield(state, -2, "message");

    lua_pop(state, 1);
}

// tas.global: the same store as tas.shared, in its own namespace.
void LuaApi::RegisterGlobalApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);
    lua_getglobal(state, "tas");
    RegisterStore(state, context, kGlobalNamespace);
    lua_setfield(state, -2, "global");
    lua_pop(state, 1);
}
