#include <gtest/gtest.h>

#include "LuaRuntime/LuaProtectedCall.h"
#include "LuaRuntime/LuaBinder.h"
#include "LuaRuntime/LuaFunction.h"
#include "LuaRuntime/LuaThread.h"
#include "LuaRuntime/LuaYieldState.h"
#include "LuaRuntime/LuaRef.h"
#include "LuaRuntime/LuaStackGuard.h"
#include "LuaRuntime/LuaState.h"
#include "LuaRuntime/LuaUserdata.h"
#include "LuaRuntime/LuaValue.h"

struct OwnedCounter {
    static int destroyed;
    int value = 0;

    explicit OwnedCounter(int v) : value(v) {}
    ~OwnedCounter() { ++destroyed; }
};

int OwnedCounter::destroyed = 0;

struct BorrowedCounter {
    int value = 0;
};

struct RegistryPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct BaseEntity {
    int id = 0;
};

struct CameraEntity : BaseEntity {
    float fov = 0.0f;
};

struct CountingYieldState {
    static int alive;
    int value = 0;

    explicit CountingYieldState(int initial) : value(initial) { ++alive; }
    ~CountingYieldState() { --alive; }
};

int CountingYieldState::alive = 0;

static int RegistryPointSum(lua_State *L) {
    auto *point = tas::lua::CheckUserdata<RegistryPoint>(L, 1, "RegistryPoint");
    lua_pushnumber(L, point->x + point->y);
    return 1;
}

static int RegistryPointIndex(lua_State *L) {
    auto *point = tas::lua::CheckUserdata<RegistryPoint>(L, 1, "RegistryPoint");
    const int index = static_cast<int>(luaL_checkinteger(L, 2));
    if (index == 0) {
        lua_pushnumber(L, point->x);
        return 1;
    }
    if (index == 1) {
        lua_pushnumber(L, point->y);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int RegistryPointNewIndex(lua_State *L) {
    auto *point = tas::lua::CheckUserdata<RegistryPoint>(L, 1, "RegistryPoint");
    const int index = static_cast<int>(luaL_checkinteger(L, 2));
    const float value = static_cast<float>(luaL_checknumber(L, 3));
    if (index == 0) {
        point->x = value;
        return 0;
    }
    if (index == 1) {
        point->y = value;
        return 0;
    }
    return luaL_error(L, "RegistryPoint index out of range");
}

static int RegistryPointAdd(lua_State *L) {
    auto *a = tas::lua::CheckUserdata<RegistryPoint>(L, 1, "RegistryPoint");
    auto *b = tas::lua::CheckUserdata<RegistryPoint>(L, 2, "RegistryPoint");
    tas::lua::PushOwnedUserdata<RegistryPoint>(L, "RegistryPoint", RegistryPoint{a->x + b->x, a->y + b->y});
    return 1;
}

static int CountingYieldContinuation(lua_State *L, int, lua_KContext ctx) {
    auto *state = tas::lua::LuaYieldState<CountingYieldState>::Get(L, ctx);
    lua_pushinteger(L, ++state->value);
    tas::lua::LuaYieldState<CountingYieldState>::Release(L, ctx);
    return 1;
}

static int YieldCountingState(lua_State *L) {
    const auto ctx = tas::lua::LuaYieldState<CountingYieldState>::Create(L, 41);
    return lua_yieldk(L, 0, ctx, CountingYieldContinuation);
}

TEST(LuaRuntimeTest, StackGuardDetectsBalancedStack) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    {
        tas::lua::LuaStackGuard guard(L);
        lua_pushinteger(L, 42);
        lua_pop(L, 1);
        EXPECT_TRUE(guard.IsBalanced());
    }
}

TEST(LuaRuntimeTest, RegistryRefMovesClonesAndReleases) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    lua_pushstring(L, "stable");
    tas::lua::LuaRef ref = tas::lua::LuaRef::FromStack(L, -1);
    EXPECT_EQ(lua_gettop(L), 0);
    EXPECT_TRUE(ref.IsValid());

    tas::lua::LuaRef clone = ref.Clone();
    ref.Reset();
    EXPECT_FALSE(ref.IsValid());
    EXPECT_TRUE(clone.IsValid());

    clone.Push();
    ASSERT_TRUE(lua_isstring(L, -1));
    EXPECT_STREQ(lua_tostring(L, -1), "stable");
    lua_pop(L, 1);

    tas::lua::LuaRef moved = std::move(clone);
    EXPECT_FALSE(clone.IsValid());
    EXPECT_TRUE(moved.IsValid());
}

TEST(LuaRuntimeTest, RegistryRefPushesOntoTargetCoroutineStack) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    lua_pushinteger(L, 21);
    tas::lua::LuaRef ref = tas::lua::LuaRef::FromStack(L, -1);
    ASSERT_TRUE(ref.IsValid());

    lua_State *thread = lua_newthread(L);
    ASSERT_NE(thread, nullptr);
    const int mainTop = lua_gettop(L);

    ref.Push(thread);
    EXPECT_EQ(lua_gettop(L), mainTop);
    ASSERT_EQ(lua_gettop(thread), 1);
    EXPECT_EQ(lua_tointeger(thread, -1), 21);
    lua_pop(thread, 1);

    lua_pop(L, 1);
}

TEST(LuaRuntimeTest, ProtectedCallReturnsTracebackOnError) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();

    auto load = state.LoadString(
        "local function inner()\n"
        "  error('boom')\n"
        "end\n"
        "return inner()\n",
        "traceback_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();

    auto result = tas::lua::ProtectedCall(state.Get(), 0, 0);
    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().message.find("boom"), std::string::npos);
    EXPECT_NE(result.GetError().message.find("traceback_test"), std::string::npos);
}

TEST(LuaRuntimeTest, LuaValueRoundTripsTablesWithoutSharingLuaObjects) {
    tas::lua::LuaState source;
    tas::lua::LuaState target;
    source.OpenStandardLibraries();
    target.OpenStandardLibraries();

    lua_State *L = source.Get();
    lua_newtable(L);
    lua_pushstring(L, "answer");
    lua_pushinteger(L, 42);
    lua_settable(L, -3);
    lua_pushstring(L, "nested");
    lua_newtable(L);
    lua_pushinteger(L, 1);
    lua_pushboolean(L, 1);
    lua_settable(L, -3);
    lua_settable(L, -3);

    auto value = tas::lua::LuaValue::FromStack(L, -1);
    ASSERT_TRUE(value.IsOk()) << value.GetError().Format();
    lua_pop(L, 1);

    value.Unwrap().Push(target.Get());
    ASSERT_TRUE(lua_istable(target.Get(), -1));
    lua_getfield(target.Get(), -1, "answer");
    EXPECT_EQ(lua_tointeger(target.Get(), -1), 42);
    lua_pop(target.Get(), 1);
    lua_getfield(target.Get(), -1, "nested");
    ASSERT_TRUE(lua_istable(target.Get(), -1));
    lua_geti(target.Get(), -1, 1);
    EXPECT_TRUE(lua_toboolean(target.Get(), -1));
    lua_pop(target.Get(), 3);
}

TEST(LuaRuntimeTest, LuaValueReadsTypedTableFieldsWithDefaults) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    auto load = state.LoadString(
        "return {\n"
        "  name = 'demo',\n"
        "  update_rate = 132.0,\n"
        "  retries = 3,\n"
        "  enabled = true\n"
        "}\n",
        "value_fields_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    ASSERT_TRUE(tas::lua::ProtectedCall(L, 0, 1).IsOk());

    auto value = tas::lua::LuaValue::FromStack(L, -1);
    ASSERT_TRUE(value.IsOk()) << value.GetError().Format();
    const auto &table = value.Unwrap();

    EXPECT_EQ(table.GetStringField("name", "fallback"), "demo");
    EXPECT_DOUBLE_EQ(table.GetNumberField("update_rate", 0.0), 132.0);
    EXPECT_EQ(table.GetIntegerField("retries", 0), 3);
    EXPECT_TRUE(table.GetBoolField("enabled", false));
    EXPECT_EQ(table.GetStringField("missing", "fallback"), "fallback");
}

TEST(LuaRuntimeTest, UserdataDistinguishesOwnedAndBorrowedObjects) {
    OwnedCounter::destroyed = 0;
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    tas::lua::RegisterUserdata<OwnedCounter>(L, "OwnedCounter");
    tas::lua::PushOwnedUserdata<OwnedCounter>(L, "OwnedCounter", 7);
    auto *owned = tas::lua::CheckUserdata<OwnedCounter>(L, -1, "OwnedCounter");
    ASSERT_NE(owned, nullptr);
    EXPECT_EQ(owned->value, 7);
    lua_pop(L, 1);
    lua_gc(L, LUA_GCCOLLECT, 0);
    EXPECT_EQ(OwnedCounter::destroyed, 1);

    BorrowedCounter borrowed{9};
    tas::lua::LuaUserdataRegistry<BorrowedCounter>(L, "BorrowedCounter")
        .Property<&BorrowedCounter::value>("value");
    tas::lua::PushBorrowedUserdata<BorrowedCounter>(L, "BorrowedCounter", &borrowed);
    auto *borrowedFromLua = tas::lua::CheckUserdata<BorrowedCounter>(L, -1, "BorrowedCounter");
    ASSERT_EQ(borrowedFromLua, &borrowed);
    lua_pop(L, 1);
    lua_gc(L, LUA_GCCOLLECT, 0);
    EXPECT_EQ(borrowed.value, 9);
}

TEST(LuaRuntimeTest, UserdataRejectsNullBorrowedPointer) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    tas::lua::LuaUserdataRegistry<BorrowedCounter>(L, "BorrowedCounter")
        .Property<&BorrowedCounter::value>("value");
    tas::lua::PushBorrowedUserdata<BorrowedCounter>(L, "BorrowedCounter", nullptr);
    lua_setglobal(L, "borrowed");

    auto load = state.LoadString(
        "return function()\n"
        "  local native = borrowed\n"
        "  return native.value\n"
        "end\n",
        "userdata_null_borrowed_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    ASSERT_TRUE(tas::lua::ProtectedCall(L, 0, 1).IsOk());

    auto result = tas::lua::ProtectedCall(L, 0, 0);
    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().message.find("BorrowedCounter"), std::string::npos);
    EXPECT_NE(result.GetError().message.find("null"), std::string::npos);
}

TEST(LuaRuntimeTest, UserdataBaseCheckerAcceptsDerivedUserdata) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    tas::lua::LuaUserdataRegistry<BaseEntity>(L, "BaseEntity")
        .ReadonlyProperty("id", [](lua_State *state) -> int {
            auto *entity = tas::lua::CheckUserdata<BaseEntity>(state, 1, "BaseEntity");
            lua_pushinteger(state, entity->id);
            return 1;
        });
    tas::lua::LuaUserdataRegistry<CameraEntity>(L, "CameraEntity")
        .Base<BaseEntity>("BaseEntity")
        .Property<&CameraEntity::fov>("fov");

    tas::lua::PushOwnedUserdata<CameraEntity>(L, "CameraEntity", CameraEntity{{7}, 65.0f});
    lua_setglobal(L, "camera");

    auto load = state.LoadString(
        "camera.fov = 70\n"
        "return camera.id, camera.fov\n",
        "userdata_base_checker_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    auto result = tas::lua::ProtectedCall(L, 0, 2);
    ASSERT_TRUE(result.IsOk()) << result.GetError().Format();
    EXPECT_EQ(lua_tointeger(L, -2), 7);
    EXPECT_DOUBLE_EQ(lua_tonumber(L, -1), 70.0);
    lua_pop(L, 2);
}

TEST(LuaRuntimeTest, UserdataRegistryDispatchesPropertiesMethodsMetamethodsAndNumericIndex) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    tas::lua::LuaUserdataRegistry<RegistryPoint>(L, "RegistryPoint")
        .Property<&RegistryPoint::x>("x")
        .Property<&RegistryPoint::y>("y")
        .ReadonlyProperty("total", RegistryPointSum)
        .Method("sum", RegistryPointSum)
        .NumericIndex(RegistryPointIndex, RegistryPointNewIndex)
        .MetaMethod("__add", RegistryPointAdd);

    tas::lua::PushOwnedUserdata<RegistryPoint>(L, "RegistryPoint", RegistryPoint{1.0f, 2.0f});
    lua_setglobal(L, "p");

    auto load = state.LoadString(
        "p.x = 4\n"
        "p[1] = 5\n"
        "local q = p + p\n"
        "return p.x, p.y, p[0], p[1], p:sum(), p.total, q.x, q.y\n",
        "userdata_registry_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    auto call = tas::lua::ProtectedCall(L, 0, 8);
    ASSERT_TRUE(call.IsOk()) << call.GetError().Format();
    EXPECT_DOUBLE_EQ(lua_tonumber(L, -8), 4.0);
    EXPECT_DOUBLE_EQ(lua_tonumber(L, -7), 5.0);
    EXPECT_DOUBLE_EQ(lua_tonumber(L, -6), 4.0);
    EXPECT_DOUBLE_EQ(lua_tonumber(L, -5), 5.0);
    EXPECT_DOUBLE_EQ(lua_tonumber(L, -4), 9.0);
    EXPECT_DOUBLE_EQ(lua_tonumber(L, -3), 9.0);
    EXPECT_DOUBLE_EQ(lua_tonumber(L, -2), 8.0);
    EXPECT_DOUBLE_EQ(lua_tonumber(L, -1), 10.0);
    lua_pop(L, 8);
}

TEST(LuaRuntimeTest, BinderCreatesTablesFunctionsAndAliases) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    tas::lua::LuaBinder binder(L);
    auto tas = binder.CreateGlobalTable("tas");
    auto keyboard = tas.CreateTable("keyboard");
    keyboard.SetFunction("answer", [](lua_State *state) -> int {
        lua_pushinteger(state, 42);
        return 1;
    });
    tas.SetAlias("answer", keyboard, "answer");

    auto load = state.LoadString("return tas.keyboard.answer(), tas.answer()", "binder_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    auto call = tas::lua::ProtectedCall(L, 0, 2);
    ASSERT_TRUE(call.IsOk()) << call.GetError().Format();
    EXPECT_EQ(lua_tointeger(L, -2), 42);
    EXPECT_EQ(lua_tointeger(L, -1), 42);
    lua_pop(L, 2);
}

TEST(LuaRuntimeTest, LuaThreadResumesYieldsAndCompletes) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    auto load = state.LoadString(
        "return function()\n"
        "  coroutine.yield('paused')\n"
        "  return 'done'\n"
        "end\n",
        "thread_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    ASSERT_TRUE(tas::lua::ProtectedCall(L, 0, 1).IsOk());

    auto thread = tas::lua::LuaThread::CreateFromFunction(L, -1);
    lua_pop(L, 1);
    ASSERT_TRUE(thread.IsValid());

    auto first = thread.Resume(0);
    ASSERT_TRUE(first.IsOk()) << first.GetError().Format();
    EXPECT_EQ(thread.Status(), tas::lua::LuaThreadStatus::Yielded);
    ASSERT_EQ(first.Unwrap(), 1);
    EXPECT_STREQ(lua_tostring(thread.State(), -1), "paused");
    lua_pop(thread.State(), 1);

    auto second = thread.Resume(0);
    ASSERT_TRUE(second.IsOk()) << second.GetError().Format();
    EXPECT_EQ(thread.Status(), tas::lua::LuaThreadStatus::Dead);
    ASSERT_EQ(second.Unwrap(), 1);
    EXPECT_STREQ(lua_tostring(thread.State(), -1), "done");
    lua_pop(thread.State(), 1);
}

TEST(LuaRuntimeTest, LuaYieldStateSurvivesYieldAndReleasesOnCompletion) {
    CountingYieldState::alive = 0;
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    lua_pushcfunction(L, YieldCountingState);
    lua_setglobal(L, "yield_counting_state");

    auto load = state.LoadString(
        "return function()\n"
        "  return yield_counting_state()\n"
        "end\n",
        "yield_state_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    ASSERT_TRUE(tas::lua::ProtectedCall(L, 0, 1).IsOk());

    auto thread = tas::lua::LuaThread::CreateFromFunction(L, -1);
    lua_pop(L, 1);

    auto first = thread.Resume(0);
    ASSERT_TRUE(first.IsOk()) << first.GetError().Format();
    EXPECT_EQ(thread.Status(), tas::lua::LuaThreadStatus::Yielded);
    EXPECT_EQ(CountingYieldState::alive, 1);

    auto second = thread.Resume(0);
    ASSERT_TRUE(second.IsOk()) << second.GetError().Format();
    EXPECT_EQ(thread.Status(), tas::lua::LuaThreadStatus::Dead);
    ASSERT_EQ(second.Unwrap(), 1);
    EXPECT_EQ(lua_tointeger(thread.State(), -1), 42);
    lua_pop(thread.State(), 1);
    EXPECT_EQ(CountingYieldState::alive, 0);
}

TEST(LuaRuntimeTest, LuaThreadReturnsTracebackOnError) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    auto load = state.LoadString(
        "return function()\n"
        "  local function inner()\n"
        "    error('thread boom')\n"
        "  end\n"
        "  inner()\n"
        "end\n",
        "thread_error_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    ASSERT_TRUE(tas::lua::ProtectedCall(L, 0, 1).IsOk());

    auto thread = tas::lua::LuaThread::CreateFromFunction(L, -1);
    lua_pop(L, 1);

    auto result = thread.Resume(0);
    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().message.find("thread boom"), std::string::npos);
    EXPECT_NE(result.GetError().message.find("thread_error_test"), std::string::npos);
    EXPECT_EQ(thread.Status(), tas::lua::LuaThreadStatus::Dead);
}

TEST(LuaRuntimeTest, LuaFunctionCallsReferencedFunctionWithTraceback) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    auto load = state.LoadString(
        "return function(a, b)\n"
        "  return a + b\n"
        "end\n",
        "function_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    ASSERT_TRUE(tas::lua::ProtectedCall(L, 0, 1).IsOk());

    auto function = tas::lua::LuaFunction::FromStack(L, -1);
    ASSERT_TRUE(function.IsValid());

    auto result = function.Call(2, 1, [](lua_State *state) {
        lua_pushinteger(state, 20);
        lua_pushinteger(state, 22);
    });
    ASSERT_TRUE(result.IsOk()) << result.GetError().Format();
    EXPECT_EQ(result.Unwrap(), 1);
    EXPECT_EQ(lua_tointeger(L, -1), 42);
    lua_pop(L, 1);
}

TEST(LuaRuntimeTest, LuaFunctionReturnsTracebackOnError) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    auto load = state.LoadString(
        "return function()\n"
        "  local function inner()\n"
        "    error('function boom')\n"
        "  end\n"
        "  inner()\n"
        "end\n",
        "function_error_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    ASSERT_TRUE(tas::lua::ProtectedCall(L, 0, 1).IsOk());

    auto function = tas::lua::LuaFunction::FromStack(L, -1);

    auto result = function.Call(0, 0);
    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().message.find("function boom"), std::string::npos);
    EXPECT_NE(result.GetError().message.find("function_error_test"), std::string::npos);
    EXPECT_EQ(lua_gettop(L), 0);
}

TEST(LuaRuntimeTest, LuaFunctionCallsWithThreeArguments) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    auto load = state.LoadString(
        "captured = {}\n"
        "return function(a, b, c)\n"
        "  captured[1] = a\n"
        "  captured[2] = b\n"
        "  captured[3] = c\n"
        "end\n",
        "function_args_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    ASSERT_TRUE(tas::lua::ProtectedCall(L, 0, 1).IsOk());

    auto function = tas::lua::LuaFunction::FromStack(L, -1);
    auto result = function.Call(3, 0, [](lua_State *state) {
        lua_pushinteger(state, 10);
        lua_pushnil(state);
        lua_pushstring(state, "score");
    });
    ASSERT_TRUE(result.IsOk()) << result.GetError().Format();

    lua_getglobal(L, "captured");
    lua_geti(L, -1, 1);
    EXPECT_EQ(lua_tointeger(L, -1), 10);
    lua_pop(L, 1);
    lua_geti(L, -1, 2);
    EXPECT_TRUE(lua_isnil(L, -1));
    lua_pop(L, 1);
    lua_geti(L, -1, 3);
    EXPECT_STREQ(lua_tostring(L, -1), "score");
    lua_pop(L, 2);
}
