#include <gtest/gtest.h>

#include "EventManager.h"
#include "LuaRuntime/LuaFunction.h"
#include "LuaRuntime/LuaProtectedCall.h"
#include "LuaRuntime/LuaState.h"

TEST(EventManagerTest, CppListenersFireAndOneTimeListenersAreRemoved) {
    EventManager events;
    int calls = 0;

    auto persistent = events.RegisterListener("tick", [&]() {
        ++calls;
    });
    auto once = events.RegisterOnceListener("tick", [&]() {
        calls += 10;
    });

    ASSERT_NE(persistent, EventManager::kInvalidListenerId);
    ASSERT_NE(once, EventManager::kInvalidListenerId);

    events.FireEvent("tick");
    EXPECT_EQ(calls, 11);
    EXPECT_EQ(events.GetListenerCount("tick"), 1);

    events.FireEvent("tick");
    EXPECT_EQ(calls, 12);
}

TEST(EventManagerTest, LuaFunctionListenerUsesRegistryReference) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    auto load = state.LoadString(
        "called = 0\n"
        "return function()\n"
        "  called = called + 1\n"
        "end\n",
        "event_manager_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    ASSERT_TRUE(tas::lua::ProtectedCall(L, 0, 1).IsOk());

    EventManager events;
    auto listener = events.RegisterListener("tick", tas::lua::LuaFunction::FromStack(L, -1));
    ASSERT_NE(listener, EventManager::kInvalidListenerId);

    events.FireEvent("tick");
    events.FireEvent("tick");

    lua_getglobal(L, "called");
    EXPECT_EQ(lua_tointeger(L, -1), 2);
    lua_pop(L, 1);
}
