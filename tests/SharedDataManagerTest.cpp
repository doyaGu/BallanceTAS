#include <gtest/gtest.h>

#include "SharedDataManager.h"
#include "LuaRuntime/LuaFunction.h"
#include "LuaRuntime/LuaProtectedCall.h"
#include "LuaRuntime/LuaState.h"
#include "LuaRuntime/LuaValue.h"

TEST(SharedDataManagerTest, BuildsSeparateGlobalAndSharedKeys) {
    EXPECT_EQ(SharedDataManager::MakeGlobalKey("score"), "global:score");
    EXPECT_EQ(SharedDataManager::MakeSharedKey("score"), "shared:score");
    EXPECT_EQ(SharedDataManager::MakeGlobalKey("  route  "), "global:route");
    EXPECT_NE(SharedDataManager::MakeGlobalKey("score"), SharedDataManager::MakeSharedKey("score"));
}

TEST(SharedDataManagerTest, ClearsOnlyMatchingNamespacePrefix) {
    SharedDataManager shared(reinterpret_cast<TASEngine *>(0x1));
    ASSERT_TRUE(shared.Initialize());

    ASSERT_TRUE(shared.Set(SharedDataManager::MakeGlobalKey("score"), tas::lua::LuaValue{static_cast<lua_Integer>(1)}));
    ASSERT_TRUE(shared.Set(SharedDataManager::MakeSharedKey("score"), tas::lua::LuaValue{static_cast<lua_Integer>(2)}));

    shared.ClearNamespace("shared:");

    EXPECT_TRUE(shared.Has(SharedDataManager::MakeGlobalKey("score")));
    EXPECT_FALSE(shared.Has(SharedDataManager::MakeSharedKey("score")));
}

TEST(SharedDataManagerTest, StoresLuaValueWithoutSharingLuaObjects) {
    SharedDataManager shared(reinterpret_cast<TASEngine *>(0x1));
    ASSERT_TRUE(shared.Initialize());

    tas::lua::LuaState source;
    tas::lua::LuaState target;
    source.OpenStandardLibraries();
    target.OpenStandardLibraries();

    auto load = source.LoadString("return {answer = 42, nested = {ok = true}}", "shared_value_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    ASSERT_TRUE(tas::lua::ProtectedCall(source.Get(), 0, 1).IsOk());

    auto value = tas::lua::LuaValue::FromStack(source.Get(), -1);
    ASSERT_TRUE(value.IsOk()) << value.GetError().Format();
    ASSERT_TRUE(shared.Set("payload", value.Unwrap()));
    lua_pop(source.Get(), 1);

    auto stored = shared.Get("payload");
    stored.Push(target.Get());
    ASSERT_TRUE(lua_istable(target.Get(), -1));
    lua_getfield(target.Get(), -1, "answer");
    EXPECT_EQ(lua_tointeger(target.Get(), -1), 42);
    lua_pop(target.Get(), 1);
    lua_getfield(target.Get(), -1, "nested");
    ASSERT_TRUE(lua_istable(target.Get(), -1));
    lua_getfield(target.Get(), -1, "ok");
    EXPECT_TRUE(lua_toboolean(target.Get(), -1));
    lua_pop(target.Get(), 3);
}

TEST(SharedDataManagerTest, RejectsUnsupportedLuaPayloadTypesBeforeStorage) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    lua_pushcfunction(L, [](lua_State *) -> int { return 0; });
    auto functionValue = tas::lua::LuaValue::FromStack(L, -1);
    EXPECT_TRUE(functionValue.IsError());
    lua_pop(L, 1);

    lua_newthread(L);
    auto threadValue = tas::lua::LuaValue::FromStack(L, -1);
    EXPECT_TRUE(threadValue.IsError());
    lua_pop(L, 1);
}

TEST(SharedDataManagerTest, WatchInvokesLuaFunctionWithNewOldAndKey) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    SharedDataManager shared(reinterpret_cast<TASEngine *>(0x1));
    ASSERT_TRUE(shared.Initialize());

    auto load = state.LoadString(
        "events = {}\n"
        "return function(new_value, old_value, key)\n"
        "  events[#events + 1] = {new_value = new_value, old_value = old_value, key = key}\n"
        "end\n",
        "shared_watch_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    ASSERT_TRUE(tas::lua::ProtectedCall(L, 0, 1).IsOk());

    shared.Watch("ctx", std::weak_ptr<ScriptContext>(), "score", tas::lua::LuaFunction::FromStack(L, -1));
    ASSERT_TRUE(shared.Set("score", tas::lua::LuaValue{static_cast<lua_Integer>(10)}));
    shared.Tick();

    lua_getglobal(L, "events");
    lua_geti(L, -1, 1);
    ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "new_value");
    EXPECT_EQ(lua_tointeger(L, -1), 10);
    lua_pop(L, 1);
    lua_getfield(L, -1, "old_value");
    EXPECT_TRUE(lua_isnil(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, -1, "key");
    EXPECT_STREQ(lua_tostring(L, -1), "score");
    lua_pop(L, 3);

    shared.Shutdown();
}
