#include <gtest/gtest.h>

#include "AsyncTask.h"
#include "LuaApi/LuaApi.h"
#include "LuaScheduler.h"
#include "LuaRuntime/LuaFunction.h"
#include "LuaRuntime/LuaProtectedCall.h"
#include "LuaRuntime/LuaThread.h"
#include "LuaRuntime/LuaState.h"
#include "ScriptContext.h"

namespace {

LuaScheduler *GetScheduler(lua_State *L) {
    return static_cast<LuaScheduler *>(lua_touserdata(L, lua_upvalueindex(1)));
}

int TestWaitTicks(lua_State *L) {
    auto *scheduler = GetScheduler(L);
    const int ticks = static_cast<int>(luaL_checkinteger(L, 1));
    scheduler->YieldTicks(ticks);
    return lua_yieldk(L, 0, 0, nullptr);
}

int TestWaitUntil(lua_State *L) {
    auto *scheduler = GetScheduler(L);
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    auto predicate = tas::lua::LuaFunction::FromStack(L, -1);
    scheduler->YieldUntil(std::move(predicate));
    return lua_yieldk(L, 0, 0, nullptr);
}

void RegisterSchedulerHelpers(lua_State *L, LuaScheduler &scheduler) {
    lua_pushlightuserdata(L, &scheduler);
    lua_pushcclosure(L, TestWaitTicks, 1);
    lua_setglobal(L, "wait_ticks");

    lua_pushlightuserdata(L, &scheduler);
    lua_pushcclosure(L, TestWaitUntil, 1);
    lua_setglobal(L, "wait_until");
}

tas::lua::LuaFunction LoadFunction(tas::lua::LuaState &state, const char *script, const char *chunkName) {
    auto load = state.LoadString(script, chunkName);
    EXPECT_TRUE(load.IsOk()) << load.GetError().Format();
    auto call = tas::lua::ProtectedCall(state.Get(), 0, 1);
    EXPECT_TRUE(call.IsOk()) << call.GetError().Format();
    return tas::lua::LuaFunction::FromStack(state.Get(), -1);
}

void RunScript(tas::lua::LuaState &state, const char *script, const char *chunkName) {
    auto load = state.LoadString(script, chunkName);
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    auto call = tas::lua::ProtectedCall(state.Get(), 0, 0);
    ASSERT_TRUE(call.IsOk()) << call.GetError().Format();
}

int EventCount(lua_State *L) {
    lua_getglobal(L, "events");
    const int count = static_cast<int>(lua_rawlen(L, -1));
    lua_pop(L, 1);
    return count;
}

const char *EventAt(lua_State *L, int index) {
    lua_getglobal(L, "events");
    lua_rawgeti(L, -1, index);
    const char *event = lua_tostring(L, -1);
    lua_pop(L, 2);
    return event;
}

} // namespace

TEST(LuaSchedulerTest, WaitTicksResumesCoroutineOnLaterTicks) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    LuaScheduler scheduler(reinterpret_cast<TASEngine *>(0x1), reinterpret_cast<ScriptContext *>(0x1));
    RegisterSchedulerHelpers(state.Get(), scheduler);

    auto function = LoadFunction(state,
        "events = {}\n"
        "return function()\n"
        "  events[#events + 1] = 'start'\n"
        "  wait_ticks(2)\n"
        "  events[#events + 1] = 'after'\n"
        "end\n",
        "scheduler_wait_ticks_test");
    scheduler.AddCoroutineTask(std::move(function));

    scheduler.Tick();
    EXPECT_EQ(EventCount(state.Get()), 1);
    EXPECT_STREQ(EventAt(state.Get(), 1), "start");
    EXPECT_TRUE(scheduler.IsRunning());

    scheduler.Tick();
    EXPECT_EQ(EventCount(state.Get()), 1);

    scheduler.Tick();
    EXPECT_EQ(EventCount(state.Get()), 2);
    EXPECT_STREQ(EventAt(state.Get(), 2), "after");
    EXPECT_FALSE(scheduler.IsRunning());
}

TEST(LuaSchedulerTest, WaitUntilRetriesPredicateAcrossTicks) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    LuaScheduler scheduler(reinterpret_cast<TASEngine *>(0x1), reinterpret_cast<ScriptContext *>(0x1));
    RegisterSchedulerHelpers(state.Get(), scheduler);

    auto function = LoadFunction(state,
        "events = {}\n"
        "gate = false\n"
        "return function()\n"
        "  wait_until(function() return gate end)\n"
        "  events[#events + 1] = 'opened'\n"
        "end\n",
        "scheduler_wait_until_test");
    scheduler.AddCoroutineTask(std::move(function));

    scheduler.Tick();
    scheduler.Tick();
    EXPECT_EQ(EventCount(state.Get()), 0);

    lua_pushboolean(state.Get(), 1);
    lua_setglobal(state.Get(), "gate");
    scheduler.Tick();

    EXPECT_EQ(EventCount(state.Get()), 1);
    EXPECT_STREQ(EventAt(state.Get(), 1), "opened");
}

TEST(LuaSchedulerTest, NestedWaitsResumeInOrderAcrossTicks) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    LuaScheduler scheduler(reinterpret_cast<TASEngine *>(0x1), reinterpret_cast<ScriptContext *>(0x1));
    RegisterSchedulerHelpers(state.Get(), scheduler);

    auto function = LoadFunction(state,
        "events = {}\n"
        "gate = false\n"
        "return function()\n"
        "  events[#events + 1] = 'start'\n"
        "  wait_ticks(1)\n"
        "  events[#events + 1] = 'after_ticks'\n"
        "  wait_until(function() return gate end)\n"
        "  events[#events + 1] = 'after_gate'\n"
        "end\n",
        "scheduler_nested_wait_test");
    scheduler.AddCoroutineTask(std::move(function));

    scheduler.Tick();
    ASSERT_EQ(EventCount(state.Get()), 1);
    EXPECT_STREQ(EventAt(state.Get(), 1), "start");

    scheduler.Tick();
    ASSERT_EQ(EventCount(state.Get()), 2);
    EXPECT_STREQ(EventAt(state.Get(), 2), "after_ticks");

    scheduler.Tick();
    EXPECT_EQ(EventCount(state.Get()), 2);

    lua_pushboolean(state.Get(), 1);
    lua_setglobal(state.Get(), "gate");
    scheduler.Tick();
    ASSERT_EQ(EventCount(state.Get()), 3);
    EXPECT_STREQ(EventAt(state.Get(), 3), "after_gate");
    EXPECT_FALSE(scheduler.IsRunning());
}

TEST(LuaSchedulerTest, CancelledAsyncTaskIsRemovedAndNotResumed) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    LuaScheduler scheduler(reinterpret_cast<TASEngine *>(0x1), reinterpret_cast<ScriptContext *>(0x1));
    RegisterSchedulerHelpers(state.Get(), scheduler);

    auto function = LoadFunction(state,
        "events = {}\n"
        "return function()\n"
        "  events[#events + 1] = 'start'\n"
        "  wait_ticks(1)\n"
        "  events[#events + 1] = 'after_cancel'\n"
        "end\n",
        "scheduler_async_cancel_test");
    ASSERT_TRUE(function.IsValid());
    function.Push();
    auto thread = tas::lua::LuaThread::CreateFromFunction(state.Get(), -1);
    lua_pop(state.Get(), 1);
    auto task = std::make_shared<AsyncTask>(&scheduler, std::move(thread), reinterpret_cast<ScriptContext *>(0x1));

    scheduler.StartAsyncTask(task);
    scheduler.Tick();
    ASSERT_EQ(EventCount(state.Get()), 1);
    EXPECT_STREQ(EventAt(state.Get(), 1), "start");
    EXPECT_TRUE(scheduler.IsRunning());

    task->Cancel();
    scheduler.Tick();
    scheduler.Tick();

    EXPECT_EQ(EventCount(state.Get()), 1);
    EXPECT_TRUE(task->IsCancelled());
    EXPECT_FALSE(scheduler.IsRunning());
    EXPECT_EQ(scheduler.GetTaskCount(), 0u);
}

TEST(LuaSchedulerTest, AsyncTaskFailureRetainsLuaErrorMessage) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    LuaScheduler scheduler(reinterpret_cast<TASEngine *>(0x1), reinterpret_cast<ScriptContext *>(0x1));

    auto function = LoadFunction(state,
        "return function()\n"
        "  error('typed await failure')\n"
        "end\n",
        "scheduler_async_error_test");
    ASSERT_TRUE(function.IsValid());
    function.Push();
    auto thread = tas::lua::LuaThread::CreateFromFunction(state.Get(), -1);
    lua_pop(state.Get(), 1);
    auto task = std::make_shared<AsyncTask>(&scheduler, std::move(thread), reinterpret_cast<ScriptContext *>(0x1));

    scheduler.StartAsyncTask(task);
    scheduler.Tick();

    ASSERT_TRUE(task->IsFailed());
    EXPECT_NE(task->GetError().find("typed await failure"), std::string::npos);
    EXPECT_FALSE(scheduler.IsRunning());
    EXPECT_EQ(scheduler.GetTaskCount(), 0u);
}

TEST(LuaSchedulerTest, InvalidScheduledAsyncTaskFailsInsteadOfHangingRunning) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    LuaScheduler scheduler(reinterpret_cast<TASEngine *>(0x1), reinterpret_cast<ScriptContext *>(0x1));

    auto function = LoadFunction(state,
        "return function()\n"
        "  return 'never'\n"
        "end\n",
        "scheduler_invalid_async_test");
    ASSERT_TRUE(function.IsValid());
    function.Push();
    auto thread = tas::lua::LuaThread::CreateFromFunction(state.Get(), -1);
    lua_pop(state.Get(), 1);
    auto task = std::make_shared<AsyncTask>(&scheduler, std::move(thread), reinterpret_cast<ScriptContext *>(0x1));

    scheduler.StartAsyncTask(task);
    ASSERT_TRUE(task->IsRunning());
    task->GetThread()->Reset();

    scheduler.Tick();

    EXPECT_TRUE(task->IsFailed());
    EXPECT_NE(task->GetError().find("invalid"), std::string::npos);
    EXPECT_FALSE(scheduler.IsRunning());
    EXPECT_EQ(scheduler.GetTaskCount(), 0u);
}

TEST(LuaSchedulerTest, AsyncApiAwaitResumesTaskCreatedByTasAsyncCall) {
    auto *engine = reinterpret_cast<TASEngine *>(0x1);
    auto *scriptContext = reinterpret_cast<ScriptContext *>(0x1);
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    LuaScheduler scheduler(engine, scriptContext);

    lua_State *L = state.Get();
    lua_newtable(L);
    lua_setglobal(L, "tas");
    LuaApi::RegisterConcurrencyApi(L, nullptr, &scheduler);
    LuaApi::RegisterAsyncApi(L, nullptr, &scheduler);

    auto function = LoadFunction(state,
        "events = {}\n"
        "return function()\n"
        "  local task = tas.async(function() return 123 end)\n"
        "  events[#events + 1] = task:is_done() and 'done' or 'pending'\n"
        "  local value = task:await()\n"
        "  events[#events + 1] = 'await:' .. tostring(value) .. ':' .. tostring(task:is_completed())\n"
        "end\n",
        "async_api_await_test");

    scheduler.AddCoroutineTask(std::move(function));
    for (int i = 0; i < 8 && scheduler.IsRunning(); ++i) {
        scheduler.Tick();
    }

    ASSERT_EQ(EventCount(state.Get()), 2);
    EXPECT_STREQ(EventAt(state.Get(), 1), "pending");
    EXPECT_STREQ(EventAt(state.Get(), 2), "await:123:true");
    EXPECT_FALSE(scheduler.IsRunning());
}

TEST(LuaSchedulerTest, YieldingApisFailClearlyOutsideSchedulerCoroutine) {
    auto *engine = reinterpret_cast<TASEngine *>(0x1);
    auto *scriptContext = reinterpret_cast<ScriptContext *>(0x1);
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    LuaScheduler scheduler(engine, scriptContext);

    lua_State *L = state.Get();
    lua_newtable(L);
    lua_setglobal(L, "tas");
    LuaApi::RegisterConcurrencyApi(L, nullptr, &scheduler);
    LuaApi::RegisterAsyncApi(L, nullptr, &scheduler);

    RunScript(state,
        "local function expect_scheduler_error(fn)\n"
        "  local ok, err = pcall(fn)\n"
        "  assert(ok == false, 'expected yielding API to fail outside scheduler coroutine')\n"
        "  local message = type(err) == 'table' and err.message or tostring(err)\n"
        "  assert(tostring(message):find('scheduler coroutine', 1, true), tostring(message))\n"
        "end\n"
        "expect_scheduler_error(function() tas.wait_ticks(1) end)\n"
        "expect_scheduler_error(function() tas.async.delay(1) end)\n"
        "expect_scheduler_error(function() tas.async.wait_until(function() return true end) end)\n"
        "expect_scheduler_error(function() tas.await(tas.async.create(function() return 1 end)) end)\n"
        "expect_scheduler_error(function() tas.async.all({ tas.async.create(function() return 1 end) }) end)\n"
        "expect_scheduler_error(function() tas.async.race({ tas.async.create(function() return 1 end) }) end)\n"
        "expect_scheduler_error(function() tas.async.any({ tas.async.create(function() return 1 end) }) end)\n",
        "yielding_api_outside_scheduler_test");

    EXPECT_FALSE(scheduler.IsRunning());
    EXPECT_EQ(scheduler.GetTaskCount(), 0u);
}

TEST(LuaSchedulerTest, AsyncAllReportsFailureIndexInInputOrder) {
    auto *engine = reinterpret_cast<TASEngine *>(0x1);
    auto *scriptContext = reinterpret_cast<ScriptContext *>(0x1);
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    LuaScheduler scheduler(engine, scriptContext);

    lua_State *L = state.Get();
    lua_newtable(L);
    lua_setglobal(L, "tas");
    LuaApi::RegisterConcurrencyApi(L, nullptr, &scheduler);
    LuaApi::RegisterAsyncApi(L, nullptr, &scheduler);

    auto function = LoadFunction(state,
        "events = {}\n"
        "return function()\n"
        "  local ok, err = pcall(function()\n"
        "    return tas.async.all({\n"
        "      tas.async.create(function() return 'unused' end),\n"
        "      tas.async.create(function() error('second task failed') end),\n"
        "    })\n"
        "  end)\n"
        "  events[#events + 1] = tostring(ok)\n"
        "  events[#events + 1] = tostring(type(err) == 'table' and err.index or nil)\n"
        "  events[#events + 1] = tostring(type(err) == 'table' and err.message or err)\n"
        "end\n",
        "async_all_index_test");

    scheduler.AddCoroutineTask(std::move(function));
    for (int i = 0; i < 8 && scheduler.IsRunning(); ++i) {
        scheduler.Tick();
    }

    ASSERT_EQ(EventCount(state.Get()), 3);
    EXPECT_STREQ(EventAt(state.Get(), 1), "false");
    EXPECT_STREQ(EventAt(state.Get(), 2), "2");
    EXPECT_NE(std::string(EventAt(state.Get(), 3)).find("second task failed"), std::string::npos);
    EXPECT_FALSE(scheduler.IsRunning());
}
