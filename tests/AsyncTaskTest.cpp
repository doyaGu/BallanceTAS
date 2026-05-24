#include <gtest/gtest.h>

#include "AsyncTask.h"
#include "LuaRuntime/LuaProtectedCall.h"
#include "LuaRuntime/LuaState.h"
#include "LuaRuntime/LuaThread.h"

TEST(AsyncTaskTest, PollCompletesThreadAndStoresResult) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    auto load = state.LoadString("return function() return 42 end", "async_task_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    ASSERT_TRUE(tas::lua::ProtectedCall(L, 0, 1).IsOk());

    AsyncTask task(nullptr, tas::lua::LuaThread::CreateFromFunction(L, -1), nullptr);
    EXPECT_TRUE(task.IsPending());
    EXPECT_FALSE(task.Poll());
    EXPECT_TRUE(task.IsCompleted());

    task.GetResult().Push(L);
    EXPECT_EQ(lua_tointeger(L, -1), 42);
    lua_pop(L, 1);
}

TEST(AsyncTaskTest, PollStoresThreadError) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    auto load = state.LoadString("return function() error('async boom') end", "async_task_error_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    ASSERT_TRUE(tas::lua::ProtectedCall(L, 0, 1).IsOk());

    AsyncTask task(nullptr, tas::lua::LuaThread::CreateFromFunction(L, -1), nullptr);
    EXPECT_FALSE(task.Poll());
    EXPECT_TRUE(task.IsFailed());
    EXPECT_NE(task.GetError().find("async boom"), std::string::npos);
}
