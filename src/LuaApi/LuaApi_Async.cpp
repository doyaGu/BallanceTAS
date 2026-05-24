#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaFunction.h"
#include "../LuaRuntime/LuaThread.h"
#include "../LuaRuntime/LuaUserdata.h"
#include "../LuaRuntime/LuaValue.h"
#include "../LuaRuntime/LuaYieldState.h"

#include "AsyncTask.h"
#include "LuaScheduler.h"
#include "ScriptContext.h"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char *kAsyncTaskMt = "BallanceTAS.AsyncTask";

using AsyncTaskHandle = std::shared_ptr<AsyncTask>;

struct AwaitState {
    explicit AwaitState(AsyncTaskHandle task) : task(std::move(task)) {}
    AsyncTaskHandle task;
};

struct TaskListState {
    explicit TaskListState(std::vector<AsyncTaskHandle> tasks) : tasks(std::move(tasks)) {}
    std::vector<AsyncTaskHandle> tasks;
};

ScriptContext *GetContext(lua_State *L) {
    return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

LuaScheduler *RequireScheduler(lua_State *L) {
    if (lua_islightuserdata(L, lua_upvalueindex(2))) {
        auto *scheduler = static_cast<LuaScheduler *>(lua_touserdata(L, lua_upvalueindex(2)));
        if (scheduler) {
            return scheduler;
        }
    }
    auto *context = GetContext(L);
    auto *scheduler = context ? context->GetScheduler() : nullptr;
    if (!scheduler) {
        luaL_error(L, "async: scheduler is not available");
    }
    return scheduler;
}

LuaScheduler *RequireYieldableScheduler(lua_State *L, const char *functionName) {
    LuaScheduler *scheduler = RequireScheduler(L);
    if (!lua_isyieldable(L) || !scheduler->CanYieldCurrentThread()) {
        luaL_error(L, "%s must be called from a scheduler coroutine", functionName);
    }
    return scheduler;
}

AsyncTaskHandle *CheckTaskHandle(lua_State *L, int index) {
    auto *box = static_cast<tas::lua::UserdataBox<AsyncTaskHandle> *>(luaL_testudata(L, index, kAsyncTaskMt));
    if (!box || !box->ptr || !*box->ptr) {
        luaL_error(L, "AsyncTask is null");
    }
    return box->ptr;
}

AsyncTask &CheckTask(lua_State *L, int index) {
    return **CheckTaskHandle(L, index);
}

AsyncTaskHandle CheckTaskShared(lua_State *L, int index) {
    return *CheckTaskHandle(L, index);
}

void PushTask(lua_State *L, AsyncTaskHandle task) {
    tas::lua::PushOwnedUserdata<AsyncTaskHandle>(L, kAsyncTaskMt, std::move(task));
}

void PushTaskResult(lua_State *L, const AsyncTask &task) {
    task.GetResult().Push(L);
}

int YieldabilityError(lua_State *L, const char *name) {
    lua_createtable(L, 0, 2);
    lua_pushstring(L, "not_yieldable");
    lua_setfield(L, -2, "kind");
    lua_pushfstring(L, "%s must be called from a scheduler coroutine", name);
    lua_setfield(L, -2, "message");
    return lua_error(L);
}

void EnsureScheduled(lua_State *L, const AsyncTaskHandle &task) {
    if (!task || task->IsDone() || task->IsScheduled()) {
        return;
    }
    LuaScheduler *scheduler = task->GetScheduler();
    if (!scheduler) {
        scheduler = RequireScheduler(L);
    }
    scheduler->StartAsyncTask(task);
}

LuaScheduler *RequireTaskScheduler(lua_State *L, const AsyncTaskHandle &task) {
    if (task && task->GetScheduler()) {
        return task->GetScheduler();
    }
    return RequireScheduler(L);
}

int PushAwaitedOutcome(lua_State *L, const AsyncTask &task, const char *context) {
    if (task.IsCompleted()) {
        lua_pushboolean(L, 1);
        PushTaskResult(L, task);
        return 2;
    }
    lua_pushboolean(L, 0);
    if (task.IsFailed()) {
        const std::string error = task.GetError();
        lua_createtable(L, 0, 3);
        lua_pushstring(L, "failed");
        lua_setfield(L, -2, "kind");
        lua_pushfstring(L, "%s failed: %s", context, error.c_str());
        lua_setfield(L, -2, "message");
        lua_pushlstring(L, error.data(), error.size());
        lua_setfield(L, -2, "traceback");
        return 2;
    }
    if (task.IsCancelled()) {
        lua_createtable(L, 0, 2);
        lua_pushstring(L, "cancelled");
        lua_setfield(L, -2, "kind");
        lua_pushfstring(L, "%s was cancelled", context);
        lua_setfield(L, -2, "message");
        return 2;
    }
    lua_createtable(L, 0, 2);
    lua_pushstring(L, "pending");
    lua_setfield(L, -2, "kind");
    lua_pushfstring(L, "%s resumed before task completion", context);
    lua_setfield(L, -2, "message");
    return 2;
}

void PushTaskErrorTable(lua_State *L, const AsyncTask &task, size_t index, const char *context) {
    lua_createtable(L, 0, 5);
    lua_pushstring(L, task.IsCancelled() ? "cancelled" : task.IsFailed() ? "failed" : "pending");
    lua_setfield(L, -2, "kind");
    lua_pushinteger(L, static_cast<lua_Integer>(index + 1));
    lua_setfield(L, -2, "index");
    if (task.IsFailed()) {
        const std::string error = task.GetError();
        lua_pushfstring(L, "%s task %d failed: %s", context, static_cast<int>(index + 1), error.c_str());
        lua_setfield(L, -2, "message");
        lua_pushlstring(L, error.data(), error.size());
        lua_setfield(L, -2, "traceback");
    } else if (task.IsCancelled()) {
        lua_pushfstring(L, "%s task %d was cancelled", context, static_cast<int>(index + 1));
        lua_setfield(L, -2, "message");
    } else {
        lua_pushfstring(L, "%s task %d resumed before completion", context, static_cast<int>(index + 1));
        lua_setfield(L, -2, "message");
    }
}

int RaiseTaskError(lua_State *L, const AsyncTask &task, size_t index, const char *context) {
    PushTaskErrorTable(L, task, index, context);
    return lua_error(L);
}

int PushIndexedTaskFailureOutcome(lua_State *L, const AsyncTask &task, size_t index, const char *context) {
    lua_pushboolean(L, 0);
    PushTaskErrorTable(L, task, index, context);
    return 2;
}

int AwaitTaskCont(lua_State *L, int, lua_KContext ctx) {
    auto *state = tas::lua::LuaYieldState<AwaitState>::TryGet(L, ctx);
    if (!state || !state->task) {
        tas::lua::LuaYieldState<AwaitState>::Release(L, ctx);
        return luaL_error(L, "AsyncTask continuation lost task handle");
    }
    if (!state->task->IsDone()) {
        RequireTaskScheduler(L, state->task)->YieldTicks(1);
        return lua_yieldk(L, 0, ctx, AwaitTaskCont);
    }

    auto task = state->task;
    tas::lua::LuaYieldState<AwaitState>::Release(L, ctx);
    return PushAwaitedOutcome(L, *task, "AsyncTask");
}

int IsPending(lua_State *L) {
    lua_pushboolean(L, CheckTask(L, 1).IsPending());
    return 1;
}

int IsRunning(lua_State *L) {
    lua_pushboolean(L, CheckTask(L, 1).IsRunning());
    return 1;
}

int IsCompleted(lua_State *L) {
    lua_pushboolean(L, CheckTask(L, 1).IsCompleted());
    return 1;
}

int IsFailed(lua_State *L) {
    lua_pushboolean(L, CheckTask(L, 1).IsFailed());
    return 1;
}

int IsCancelled(lua_State *L) {
    lua_pushboolean(L, CheckTask(L, 1).IsCancelled());
    return 1;
}

int IsDone(lua_State *L) {
    lua_pushboolean(L, CheckTask(L, 1).IsDone());
    return 1;
}

int GetResult(lua_State *L) {
    PushTaskResult(L, CheckTask(L, 1));
    return 1;
}

int GetError(lua_State *L) {
    const std::string error = CheckTask(L, 1).GetError();
    lua_pushlstring(L, error.data(), error.size());
    return 1;
}

int Start(lua_State *L) {
    EnsureScheduled(L, CheckTaskShared(L, 1));
    return 0;
}

int Cancel(lua_State *L) {
    CheckTask(L, 1).Cancel();
    return 0;
}

int AwaitTaskRaw(lua_State *L) {
    auto task = CheckTaskShared(L, 1);
    if (task->IsDone()) {
        return PushAwaitedOutcome(L, *task, "AsyncTask");
    }
    if (!lua_isyieldable(L) || !RequireTaskScheduler(L, task)->CanYieldCurrentThread()) {
        return YieldabilityError(L, "AsyncTask.await");
    }

    EnsureScheduled(L, task);
    const auto ctx = tas::lua::LuaYieldState<AwaitState>::Create(L, task);
    RequireTaskScheduler(L, task)->YieldTicks(1);
    return lua_yieldk(L, 0, ctx, AwaitTaskCont);
}

int AwaitTask(lua_State *L) {
    return AwaitTaskRaw(L);
}

int ThenTask(lua_State *L) {
    auto &task = CheckTask(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (!task.IsCompleted()) {
        return luaL_error(L, "AsyncTask.then requires a completed task");
    }
    lua_pushvalue(L, 2);
    PushTaskResult(L, task);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        return lua_error(L);
    }
    return 1;
}

int CatchTask(lua_State *L) {
    auto &task = CheckTask(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (task.IsCompleted()) {
        PushTaskResult(L, task);
        return 1;
    }
    lua_pushvalue(L, 2);
    const std::string error = task.IsFailed() ? task.GetError() : "Task was cancelled";
    lua_pushlstring(L, error.data(), error.size());
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        return lua_error(L);
    }
    return 1;
}

int CreateTask(lua_State *L) {
    auto *context = GetContext(L);
    auto *scheduler = RequireScheduler(L);
    luaL_checktype(L, 1, LUA_TFUNCTION);
    auto thread = tas::lua::LuaThread::CreateFromFunction(L, 1);
    if (!thread.IsValid()) {
        return luaL_error(L, "async.create: failed to create coroutine");
    }
    PushTask(L, std::make_shared<AsyncTask>(scheduler, std::move(thread), context));
    return 1;
}

int AsyncCall(lua_State *L) {
    lua_remove(L, 1);
    return CreateTask(L);
}

int SpawnTask(lua_State *L) {
    CreateTask(L);
    EnsureScheduled(L, CheckTaskShared(L, -1));
    return 1;
}

int Delay(lua_State *L) {
    const int ticks = static_cast<int>(luaL_checkinteger(L, 1));
    if (ticks <= 0) {
        return luaL_error(L, "async.delay: ticks must be positive");
    }
    RequireYieldableScheduler(L, "tas.async.delay")->YieldTicks(ticks);
    return lua_yieldk(L, 0, 0, nullptr);
}

int WaitForEvent(lua_State *L) {
    const char *eventName = luaL_checkstring(L, 1);
    RequireYieldableScheduler(L, "tas.async.wait_for_event")->YieldWaitForEvent(eventName);
    return lua_yieldk(L, 0, 0, nullptr);
}

int WaitUntil(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    auto predicate = tas::lua::LuaFunction::FromStack(L, -1);
    RequireYieldableScheduler(L, "tas.async.wait_until")->YieldUntil(std::move(predicate));
    return lua_yieldk(L, 0, 0, nullptr);
}

std::vector<AsyncTaskHandle> CollectTasks(lua_State *L, int tableIndex, const char *name, bool allowEmpty) {
    luaL_checktype(L, tableIndex, LUA_TTABLE);
    tableIndex = lua_absindex(L, tableIndex);

    const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(L, tableIndex));
    if (count == 0 && !allowEmpty) {
        luaL_error(L, "%s: expected at least one task", name);
    }

    for (lua_Integer i = 1; i <= count; ++i) {
        lua_rawgeti(L, tableIndex, i);
        auto *box = static_cast<tas::lua::UserdataBox<AsyncTaskHandle> *>(luaL_testudata(L, -1, kAsyncTaskMt));
        const bool valid = box && box->ptr && *box->ptr;
        lua_pop(L, 1);
        if (!valid) {
            luaL_error(L, "%s: element %d is not an AsyncTask", name, static_cast<int>(i));
        }
    }

    std::vector<AsyncTaskHandle> tasks;
    tasks.reserve(static_cast<size_t>(count));
    for (lua_Integer i = 1; i <= count; ++i) {
        lua_rawgeti(L, tableIndex, i);
        auto *box = static_cast<tas::lua::UserdataBox<AsyncTaskHandle> *>(luaL_testudata(L, -1, kAsyncTaskMt));
        tasks.push_back(*box->ptr);
        lua_pop(L, 1);
    }
    return tasks;
}

void EnsureAllScheduled(lua_State *L, const std::vector<AsyncTaskHandle> &tasks) {
    for (const auto &task : tasks) {
        EnsureScheduled(L, task);
    }
}

bool AreAllDone(const std::vector<AsyncTaskHandle> &tasks) {
    for (const auto &task : tasks) {
        if (task && !task->IsDone()) {
            return false;
        }
    }
    return true;
}

bool IsAnyDone(const std::vector<AsyncTaskHandle> &tasks) {
    for (const auto &task : tasks) {
        if (task && task->IsDone()) {
            return true;
        }
    }
    return false;
}

bool IsAnyCompletedOrAllDone(const std::vector<AsyncTaskHandle> &tasks) {
    bool sawPending = false;
    for (const auto &task : tasks) {
        if (!task) {
            continue;
        }
        if (task->IsCompleted()) {
            return true;
        }
        if (!task->IsDone()) {
            sawPending = true;
        }
    }
    return !sawPending;
}

void CancelOtherTasks(const std::vector<AsyncTaskHandle> &tasks, size_t winnerIndex) {
    for (size_t i = 0; i < tasks.size(); ++i) {
        if (i != winnerIndex && tasks[i] && !tasks[i]->IsDone()) {
            tasks[i]->Cancel();
        }
    }
}

int PushAllOutcome(lua_State *L, const std::vector<AsyncTaskHandle> &tasks) {
    for (size_t i = 0; i < tasks.size(); ++i) {
        const auto &task = tasks[i];
        if (!task) {
            return luaL_error(L, "async.all: invalid task");
        }
        if (!task->IsCompleted()) {
            return PushIndexedTaskFailureOutcome(L, *task, i, "async.all");
        }
    }

    lua_pushboolean(L, 1);
    lua_createtable(L, static_cast<int>(tasks.size()), 0);
    for (size_t i = 0; i < tasks.size(); ++i) {
        const auto &task = tasks[i];
        PushTaskResult(L, *task);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 2;
}

int AllCont(lua_State *L, int, lua_KContext ctx) {
    auto *state = tas::lua::LuaYieldState<TaskListState>::TryGet(L, ctx);
    if (!state) {
        tas::lua::LuaYieldState<TaskListState>::Release(L, ctx);
        return luaL_error(L, "async.all continuation lost task list");
    }
    if (!AreAllDone(state->tasks)) {
        RequireTaskScheduler(L, state->tasks.empty() ? AsyncTaskHandle{} : state->tasks.front())->YieldTicks(1);
        return lua_yieldk(L, 0, ctx, AllCont);
    }

    auto tasks = state->tasks;
    tas::lua::LuaYieldState<TaskListState>::Release(L, ctx);
    return PushAllOutcome(L, tasks);
}

int AllRaw(lua_State *L) {
    auto tasks = CollectTasks(L, 1, "async.all", true);
    if (AreAllDone(tasks)) {
        return PushAllOutcome(L, tasks);
    }
    LuaScheduler *scheduler = RequireTaskScheduler(L, tasks.empty() ? AsyncTaskHandle{} : tasks.front());
    if (!lua_isyieldable(L) || !scheduler->CanYieldCurrentThread()) {
        std::vector<AsyncTaskHandle>().swap(tasks);
        return YieldabilityError(L, "async.all");
    }

    EnsureAllScheduled(L, tasks);
    const auto ctx = tas::lua::LuaYieldState<TaskListState>::Create(L, std::move(tasks));
    scheduler->YieldTicks(1);
    return lua_yieldk(L, 0, ctx, AllCont);
}

int PushRaceOutcome(lua_State *L, const std::vector<AsyncTaskHandle> &tasks, const char *context) {
    for (size_t i = 0; i < tasks.size(); ++i) {
        const auto &task = tasks[i];
        if (!task || !task->IsDone()) {
            continue;
        }
        CancelOtherTasks(tasks, i);
        if (!task->IsCompleted()) {
            return PushIndexedTaskFailureOutcome(L, *task, i, context);
        }
        lua_pushboolean(L, 1);
        lua_createtable(L, 1, 3);
        lua_pushinteger(L, static_cast<lua_Integer>(i + 1));
        lua_setfield(L, -2, "index");
        lua_pushstring(L, "completed");
        lua_setfield(L, -2, "status");
        PushTaskResult(L, *task);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "value");
        lua_rawseti(L, -2, 1);
        return 2;
    }
    return luaL_error(L, "%s: resumed before any task completed", context);
}

int RaceCont(lua_State *L, int, lua_KContext ctx) {
    auto *state = tas::lua::LuaYieldState<TaskListState>::TryGet(L, ctx);
    if (!state) {
        tas::lua::LuaYieldState<TaskListState>::Release(L, ctx);
        return luaL_error(L, "async.race continuation lost task list");
    }
    if (!IsAnyDone(state->tasks)) {
        RequireTaskScheduler(L, state->tasks.empty() ? AsyncTaskHandle{} : state->tasks.front())->YieldTicks(1);
        return lua_yieldk(L, 0, ctx, RaceCont);
    }

    auto tasks = state->tasks;
    tas::lua::LuaYieldState<TaskListState>::Release(L, ctx);
    return PushRaceOutcome(L, tasks, "async.race");
}

int RaceRaw(lua_State *L) {
    auto tasks = CollectTasks(L, 1, "async.race", false);
    if (IsAnyDone(tasks)) {
        return PushRaceOutcome(L, tasks, "async.race");
    }
    LuaScheduler *scheduler = RequireTaskScheduler(L, tasks.front());
    if (!lua_isyieldable(L) || !scheduler->CanYieldCurrentThread()) {
        std::vector<AsyncTaskHandle>().swap(tasks);
        return YieldabilityError(L, "async.race");
    }

    EnsureAllScheduled(L, tasks);
    const auto ctx = tas::lua::LuaYieldState<TaskListState>::Create(L, std::move(tasks));
    scheduler->YieldTicks(1);
    return lua_yieldk(L, 0, ctx, RaceCont);
}

int PushAnyOutcome(lua_State *L, const std::vector<AsyncTaskHandle> &tasks) {
    std::vector<size_t> errorIndexes;
    bool sawPending = false;
    for (size_t i = 0; i < tasks.size(); ++i) {
        const auto &task = tasks[i];
        if (!task) {
            continue;
        }
        if (task->IsCompleted()) {
            CancelOtherTasks(tasks, i);
            lua_pushboolean(L, 1);
            lua_createtable(L, 1, 3);
            lua_pushinteger(L, static_cast<lua_Integer>(i + 1));
            lua_setfield(L, -2, "index");
            lua_pushstring(L, "completed");
            lua_setfield(L, -2, "status");
            PushTaskResult(L, *task);
            lua_pushvalue(L, -1);
            lua_setfield(L, -3, "value");
            lua_rawseti(L, -2, 1);
            return 2;
        }
        if (task->IsFailed() || task->IsCancelled()) {
            errorIndexes.push_back(i);
        } else if (!task->IsDone()) {
            sawPending = true;
        }
    }
    if (sawPending) {
        return luaL_error(L, "async.any: resumed before any task completed");
    }

    lua_pushboolean(L, 0);
    lua_createtable(L, 0, 3);
    lua_pushstring(L, "all_failed");
    lua_setfield(L, -2, "kind");
    lua_pushstring(L, "async.any: all tasks failed or were cancelled");
    lua_setfield(L, -2, "message");
    lua_createtable(L, static_cast<int>(errorIndexes.size()), 0);
    int outIndex = 1;
    for (size_t taskIndex : errorIndexes) {
        PushTaskErrorTable(L, *tasks[taskIndex], taskIndex, "async.any");
        lua_rawseti(L, -2, outIndex++);
    }
    lua_setfield(L, -2, "errors");
    return 2;
}

int AnyCont(lua_State *L, int, lua_KContext ctx) {
    auto *state = tas::lua::LuaYieldState<TaskListState>::TryGet(L, ctx);
    if (!state) {
        tas::lua::LuaYieldState<TaskListState>::Release(L, ctx);
        return luaL_error(L, "async.any continuation lost task list");
    }
    if (!IsAnyCompletedOrAllDone(state->tasks)) {
        RequireTaskScheduler(L, state->tasks.empty() ? AsyncTaskHandle{} : state->tasks.front())->YieldTicks(1);
        return lua_yieldk(L, 0, ctx, AnyCont);
    }

    auto tasks = state->tasks;
    tas::lua::LuaYieldState<TaskListState>::Release(L, ctx);
    return PushAnyOutcome(L, tasks);
}

int AnyRaw(lua_State *L) {
    auto tasks = CollectTasks(L, 1, "async.any", false);
    if (IsAnyCompletedOrAllDone(tasks)) {
        return PushAnyOutcome(L, tasks);
    }
    LuaScheduler *scheduler = RequireTaskScheduler(L, tasks.front());
    if (!lua_isyieldable(L) || !scheduler->CanYieldCurrentThread()) {
        std::vector<AsyncTaskHandle>().swap(tasks);
        return YieldabilityError(L, "async.any");
    }

    EnsureAllScheduled(L, tasks);
    const auto ctx = tas::lua::LuaYieldState<TaskListState>::Create(L, std::move(tasks));
    scheduler->YieldTicks(1);
    return lua_yieldk(L, 0, ctx, AnyCont);
}

int AwaitTicks(lua_State *L) {
    const int ticks = static_cast<int>(luaL_checkinteger(L, 1));
    if (ticks <= 0) {
        return luaL_error(L, "await: ticks must be positive");
    }
    RequireYieldableScheduler(L, "tas.await")->YieldTicks(ticks);
    return lua_yieldk(L, 0, 0, nullptr);
}

int AwaitRaw(lua_State *L) {
    if (luaL_testudata(L, 1, kAsyncTaskMt)) {
        return AwaitTaskRaw(L);
    }
    if (lua_isinteger(L, 1)) {
        return AwaitTicks(L);
    }
    return luaL_error(L, "await requires AsyncTask or integer ticks");
}

void SetContextFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context, LuaScheduler *scheduler) {
    lua_pushlightuserdata(L, context);
    lua_pushlightuserdata(L, scheduler);
    lua_pushcclosure(L, function, 2);
    lua_setfield(L, -2, name);
}

bool RunAsyncInstallChunk(lua_State *L, const char *chunk, const char *name) {
    if (luaL_loadbuffer(L, chunk, std::strlen(chunk), name) != LUA_OK) {
        const char *message = lua_tostring(L, -1);
        std::string error = message ? message : "unknown Lua wrapper load error";
        lua_pop(L, 1);
        throw std::runtime_error(std::string(name) + ": " + error);
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char *message = lua_tostring(L, -1);
        std::string error = message ? message : "unknown Lua wrapper install error";
        lua_pop(L, 1);
        throw std::runtime_error(std::string(name) + ": " + error);
    }
    return true;
}

void InstallTaskAwaitWrapper(lua_State *L, ScriptContext *context, LuaScheduler *scheduler) {
    luaL_getmetatable(L, kAsyncTaskMt);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    lua_getfield(L, -1, tas::lua::detail::kMethodsField);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        return;
    }

    lua_pushlightuserdata(L, context);
    lua_pushlightuserdata(L, scheduler);
    lua_pushcclosure(L, AwaitTaskRaw, 2);
    lua_setfield(L, -2, "__await_raw");

    constexpr const char *chunk =
        "local mt = debug.getregistry()['BallanceTAS.AsyncTask']\n"
        "local methods = mt.__tas_methods\n"
        "local raw = methods.__await_raw\n"
        "methods.await = function(self)\n"
        "  local ok, value = raw(self)\n"
        "  if ok then return value end\n"
        "  error(value, 0)\n"
        "end\n";
    RunAsyncInstallChunk(L, chunk, "BallanceTAS.AsyncTask.await.wrapper");
    lua_pop(L, 2);
}

void InstallAsyncWrappers(lua_State *L) {
    constexpr const char *chunk =
        "local tas = tas\n"
        "local async = tas.async\n"
        "local function unwrap(ok, value)\n"
        "  if ok then return value end\n"
        "  error(value, 0)\n"
        "end\n"
        "tas.await = function(value)\n"
        "  if type(value) == 'number' then return tas.__await_ticks(value) end\n"
        "  return unwrap(tas.__await_raw(value))\n"
        "end\n"
        "async.all = function(tasks) return unwrap(async.__all_raw(tasks)) end\n"
        "async.race = function(tasks) return unwrap(async.__race_raw(tasks)) end\n"
        "async.any = function(tasks) return unwrap(async.__any_raw(tasks)) end\n";
    RunAsyncInstallChunk(L, chunk, "BallanceTAS.async.wrapper");
}

void RegisterAsyncTable(lua_State *L, ScriptContext *context, LuaScheduler *scheduler) {
    lua_getglobal(L, "tas");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "tas");
    }

    lua_newtable(L);
    SetContextFunction(L, "create", CreateTask, context, scheduler);
    SetContextFunction(L, "spawn", SpawnTask, context, scheduler);
    SetContextFunction(L, "delay", Delay, context, scheduler);
    SetContextFunction(L, "wait_for_event", WaitForEvent, context, scheduler);
    SetContextFunction(L, "wait_until", WaitUntil, context, scheduler);
    SetContextFunction(L, "__all_raw", AllRaw, context, scheduler);
    SetContextFunction(L, "__race_raw", RaceRaw, context, scheduler);
    SetContextFunction(L, "__any_raw", AnyRaw, context, scheduler);

    lua_newtable(L);
    lua_pushlightuserdata(L, context);
    lua_pushlightuserdata(L, scheduler);
    lua_pushcclosure(L, AsyncCall, 2);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);

    lua_setfield(L, -2, "async");
    SetContextFunction(L, "__await_raw", AwaitRaw, context, scheduler);
    SetContextFunction(L, "__await_ticks", AwaitTicks, context, scheduler);
    InstallAsyncWrappers(L);
    lua_pop(L, 1);
}

} // namespace

void LuaApi::RegisterAsyncApi(lua_State *state, ScriptContext *context, LuaScheduler *scheduler) {
    if (!state || (!context && !scheduler)) {
        throw std::runtime_error("LuaApi::RegisterAsyncApi requires a valid Lua state and ScriptContext or LuaScheduler");
    }

    tas::lua::LuaStackGuard guard(state);
    tas::lua::LuaUserdataRegistry<AsyncTaskHandle>(state, kAsyncTaskMt)
        .Method("is_pending", IsPending)
        .Method("is_running", IsRunning)
        .Method("is_completed", IsCompleted)
        .Method("is_failed", IsFailed)
        .Method("is_cancelled", IsCancelled)
        .Method("is_done", IsDone)
        .Method("get_result", GetResult)
        .Method("get_error", GetError)
        .Method("start", Start)
        .Method("cancel", Cancel)
        .Method("then", ThenTask)
        .Method("catch", CatchTask);

    InstallTaskAwaitWrapper(state, context, scheduler);
    RegisterAsyncTable(state, context, scheduler);
}
