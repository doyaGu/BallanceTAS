#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaFunction.h"
#include "../LuaRuntime/LuaRef.h"
#include "../LuaRuntime/LuaThread.h"
#include "../LuaRuntime/LuaUserdata.h"
#include "../LuaRuntime/LuaValue.h"
#include "../LuaRuntime/LuaYieldState.h"

#include "AsyncTask.h"
#include "LuaApiCommon.h"
#include "LuaScheduler.h"
#include "ScriptContext.h"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

constexpr const char *kAsyncTaskMt = "BallanceTAS.AsyncTask";
// Error tables raised or returned by async functions share this metatable, so
// tostring(err) and error reporting show their message.
constexpr const char *kAsyncErrorMt = "BallanceTAS.AsyncError";

using AsyncTaskHandle = std::shared_ptr<AsyncTask>;

const char *const LuaApi::kRunTaskNowKey = "BallanceTAS.RunTaskNow";

struct AwaitState {
    explicit AwaitState(AsyncTaskHandle task) : task(std::move(task)) {}
    AsyncTaskHandle task;
};

struct TaskListState {
    explicit TaskListState(tas::lua::LuaRef taskTable) : taskTable(std::move(taskTable)) {}
    tas::lua::LuaRef taskTable;
};

static ScriptContext *GetContext(lua_State *L) {
    return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

static LuaScheduler *RequireScheduler(lua_State *L) {
    if (lua_islightuserdata(L, lua_upvalueindex(2))) {
        auto *scheduler = static_cast<LuaScheduler *>(lua_touserdata(L, lua_upvalueindex(2)));
        if (scheduler) {
            return scheduler;
        }
    }
    auto *context = GetContext(L);
    auto *scheduler = context ? context->GetScheduler() : nullptr;
    if (!scheduler) {
        luaL_error(L, "tas.async: the scheduler is not available");
    }
    return scheduler;
}

static LuaScheduler *RequireYieldableScheduler(lua_State *L, const char *functionName) {
    LuaScheduler *scheduler = RequireScheduler(L);
    if (!lua_isyieldable(L) || !scheduler->CanYieldCurrentThread()) {
        luaL_error(L, "%s: waits, so it must be called from a script coroutine (main, a task or a listener)", functionName);
    }
    return scheduler;
}

static AsyncTaskHandle *CheckTaskHandle(lua_State *L, int index) {
    auto *box = static_cast<tas::lua::UserdataBox<AsyncTaskHandle> *>(luaL_testudata(L, index, kAsyncTaskMt));
    if (!box || !box->ptr || !*box->ptr) {
        luaL_error(L, "AsyncTask is null");
    }
    return box->ptr;
}

static AsyncTask &CheckTask(lua_State *L, int index) {
    return **CheckTaskHandle(L, index);
}

static AsyncTaskHandle CheckTaskShared(lua_State *L, int index) {
    return *CheckTaskHandle(L, index);
}

static void PushTask(lua_State *L, AsyncTaskHandle task) {
    tas::lua::PushOwnedUserdata<AsyncTaskHandle>(L, kAsyncTaskMt, std::move(task));
}

static void PushTaskResult(lua_State *L, const AsyncTask &task) {
    task.GetResult().Push(L);
}

static void MarkAsyncError(lua_State *L) {
    luaL_getmetatable(L, kAsyncErrorMt);
    lua_setmetatable(L, -2);
}

static int AsyncErrorToString(lua_State *L) {
    lua_getfield(L, 1, "message");
    if (!lua_isstring(L, -1)) {
        lua_pop(L, 1);
        lua_pushliteral(L, "async error");
    }
    return 1;
}

static int YieldabilityError(lua_State *L, const char *name) {
    lua_createtable(L, 0, 2);
    lua_pushstring(L, "not_yieldable");
    lua_setfield(L, -2, "kind");
    luaL_where(L, 1);
    lua_pushfstring(L, "%s: waits, so it must be called from a script coroutine (main, a task or a listener)", name);
    lua_concat(L, 2);
    lua_setfield(L, -2, "message");
    MarkAsyncError(L);
    return lua_error(L);
}

static void EnsureScheduled(lua_State *L, const AsyncTaskHandle &task) {
    if (!task || task->IsDone() || task->IsScheduled()) {
        return;
    }
    LuaScheduler *scheduler = task->GetScheduler();
    if (!scheduler) {
        scheduler = RequireScheduler(L);
    }
    scheduler->StartAsyncTask(task);
}

static LuaScheduler *RequireTaskScheduler(lua_State *L, const AsyncTaskHandle &task) {
    if (task && task->GetScheduler()) {
        return task->GetScheduler();
    }
    return RequireScheduler(L);
}

static int PushAwaitedOutcome(lua_State *L, const AsyncTask &task, const char *context) {
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
        MarkAsyncError(L);
        return 2;
    }
    if (task.IsCancelled()) {
        lua_createtable(L, 0, 2);
        lua_pushstring(L, "cancelled");
        lua_setfield(L, -2, "kind");
        lua_pushfstring(L, "%s was cancelled", context);
        lua_setfield(L, -2, "message");
        MarkAsyncError(L);
        return 2;
    }
    lua_createtable(L, 0, 2);
    lua_pushstring(L, "pending");
    lua_setfield(L, -2, "kind");
    lua_pushfstring(L, "%s resumed before task completion", context);
    lua_setfield(L, -2, "message");
    MarkAsyncError(L);
    return 2;
}

static void PushTaskErrorTable(lua_State *L, const AsyncTask &task, size_t index, const char *context) {
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
    MarkAsyncError(L);
}

static int RaiseTaskError(lua_State *L, const AsyncTask &task, size_t index, const char *context) {
    PushTaskErrorTable(L, task, index, context);
    return lua_error(L);
}

static int PushIndexedTaskFailureOutcome(lua_State *L, const AsyncTask &task, size_t index, const char *context) {
    lua_pushboolean(L, 0);
    PushTaskErrorTable(L, task, index, context);
    return 2;
}

static int AwaitTaskCont(lua_State *L, int, lua_KContext ctx) {
    auto *state = tas::lua::LuaYieldState<AwaitState>::TryGet(L, ctx);
    if (!state || !state->task) {
        tas::lua::LuaYieldState<AwaitState>::Release(L, ctx);
        return luaL_error(L, "AsyncTask:await: lost its task");
    }
    if (!state->task->IsDone()) {
        RequireTaskScheduler(L, state->task)->YieldTicks(1);
        return lua_yieldk(L, 0, ctx, AwaitTaskCont);
    }

    auto task = state->task;
    tas::lua::LuaYieldState<AwaitState>::Release(L, ctx);
    return PushAwaitedOutcome(L, *task, "AsyncTask");
}

static int IsPending(lua_State *L) {
    lua_pushboolean(L, CheckTask(L, 1).IsPending());
    return 1;
}

static int IsRunning(lua_State *L) {
    lua_pushboolean(L, CheckTask(L, 1).IsRunning());
    return 1;
}

static int IsCompleted(lua_State *L) {
    lua_pushboolean(L, CheckTask(L, 1).IsCompleted());
    return 1;
}

static int IsFailed(lua_State *L) {
    lua_pushboolean(L, CheckTask(L, 1).IsFailed());
    return 1;
}

static int IsCancelled(lua_State *L) {
    lua_pushboolean(L, CheckTask(L, 1).IsCancelled());
    return 1;
}

static int IsDone(lua_State *L) {
    lua_pushboolean(L, CheckTask(L, 1).IsDone());
    return 1;
}

static int GetResult(lua_State *L) {
    PushTaskResult(L, CheckTask(L, 1));
    return 1;
}

// task:get_error() -> message | nil (no error)
static int GetError(lua_State *L) {
    const AsyncTask &task = CheckTask(L, 1);
    if (!task.IsFailed()) {
        lua_pushnil(L);
        return 1;
    }
    const std::string error = task.GetError();
    lua_pushlstring(L, error.data(), error.size());
    return 1;
}

static int Start(lua_State *L) {
    EnsureScheduled(L, CheckTaskShared(L, 1));
    return 0;
}

static int Cancel(lua_State *L) {
    CheckTask(L, 1).Cancel();
    return 0;
}

static int AwaitTaskRaw(lua_State *L) {
    AsyncTask &task = CheckTask(L, 1);
    if (task.IsDone()) {
        return PushAwaitedOutcome(L, task, "AsyncTask");
    }
    LuaScheduler *scheduler = task.GetScheduler() ? task.GetScheduler() : RequireScheduler(L);
    if (!lua_isyieldable(L) || !scheduler->CanYieldCurrentThread()) {
        return YieldabilityError(L, "AsyncTask:await");
    }

    lua_KContext ctx = 0;
    {
        // lua_yieldk longjmps (Lua is built as C): release the handle before yielding.
        AsyncTaskHandle handle = CheckTaskShared(L, 1);
        EnsureScheduled(L, handle);
        ctx = tas::lua::LuaYieldState<AwaitState>::Create(L, handle);
    }
    scheduler->YieldTicks(1);
    return lua_yieldk(L, 0, ctx, AwaitTaskCont);
}

static int AwaitTask(lua_State *L) {
    return AwaitTaskRaw(L);
}

static int ThenTask(lua_State *L) {
    auto &task = CheckTask(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (!task.IsCompleted()) {
        return luaL_error(L, "AsyncTask:then: the task has not completed");
    }
    lua_pushvalue(L, 2);
    PushTaskResult(L, task);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        return lua_error(L);
    }
    return 1;
}

static int CatchTask(lua_State *L) {
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

static int CreateTask(lua_State *L) {
    auto *context = GetContext(L);
    auto *scheduler = RequireScheduler(L);
    luaL_checktype(L, 1, LUA_TFUNCTION);
    auto thread = tas::lua::LuaThread::CreateFromFunction(L, 1);
    if (!thread.IsValid()) {
        return luaL_error(L, "tas.async.create: failed to create coroutine");
    }
    PushTask(L, std::make_shared<AsyncTask>(scheduler, std::move(thread), context));
    return 1;
}

static int AsyncCall(lua_State *L) {
    lua_remove(L, 1);
    return CreateTask(L);
}

static int SpawnTask(lua_State *L) {
    CreateTask(L);
    EnsureScheduled(L, CheckTaskShared(L, -1));
    return 1;
}

// run_task_now(fn): create a task and run it until its first wait (tas.on listeners).
static int RunNow(lua_State *L) {
    CreateTask(L);
    auto task = CheckTaskShared(L, -1);
    RequireTaskScheduler(L, task)->RunAsyncTaskNow(task);
    return 1;
}

static std::vector<AsyncTaskHandle> CollectTasks(lua_State *L, int tableIndex, const char *name, bool allowEmpty) {
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

static tas::lua::LuaRef ReferenceTaskTable(lua_State *L, int tableIndex) {
    tableIndex = lua_absindex(L, tableIndex);
    lua_pushvalue(L, tableIndex);
    return tas::lua::LuaRef::FromStack(L, -1);
}

static std::vector<AsyncTaskHandle> CollectStateTasks(lua_State *L,
                                                     const TaskListState &state,
                                                     const char *name,
                                                     bool allowEmpty) {
    state.taskTable.Push(L);
    auto tasks = CollectTasks(L, -1, name, allowEmpty);
    lua_pop(L, 1);
    return tasks;
}

template <typename DonePredicate>
static LuaScheduler *SchedulerWhilePending(lua_State *L,
                                           const TaskListState &state,
                                           const char *name,
                                           bool allowEmpty,
                                           DonePredicate isDone) {
    auto tasks = CollectStateTasks(L, state, name, allowEmpty);
    if (isDone(tasks)) {
        return nullptr;
    }
    return RequireTaskScheduler(L, tasks.empty() ? AsyncTaskHandle{} : tasks.front());
}

static void EnsureAllScheduled(lua_State *L, const std::vector<AsyncTaskHandle> &tasks) {
    for (const auto &task : tasks) {
        EnsureScheduled(L, task);
    }
}

static bool AreAllDone(const std::vector<AsyncTaskHandle> &tasks) {
    for (const auto &task : tasks) {
        if (task && !task->IsDone()) {
            return false;
        }
    }
    return true;
}

static bool IsAnyDone(const std::vector<AsyncTaskHandle> &tasks) {
    for (const auto &task : tasks) {
        if (task && task->IsDone()) {
            return true;
        }
    }
    return false;
}

static bool IsAnyCompletedOrAllDone(const std::vector<AsyncTaskHandle> &tasks) {
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

static void CancelOtherTasks(const std::vector<AsyncTaskHandle> &tasks, size_t winnerIndex) {
    for (size_t i = 0; i < tasks.size(); ++i) {
        if (i != winnerIndex && tasks[i] && !tasks[i]->IsDone()) {
            tasks[i]->Cancel();
        }
    }
}

static int PushAllOutcome(lua_State *L, const std::vector<AsyncTaskHandle> &tasks) {
    for (size_t i = 0; i < tasks.size(); ++i) {
        const auto &task = tasks[i];
        if (!task) {
            return luaL_error(L, "tas.async.all: invalid task");
        }
        if (!task->IsCompleted()) {
            return PushIndexedTaskFailureOutcome(L, *task, i, "tas.async.all");
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

static int AllCont(lua_State *L, int, lua_KContext ctx) {
    auto *state = tas::lua::LuaYieldState<TaskListState>::TryGet(L, ctx);
    if (!state) {
        tas::lua::LuaYieldState<TaskListState>::Release(L, ctx);
        return luaL_error(L, "tas.async.all continuation lost task list");
    }
    if (auto *scheduler = SchedulerWhilePending(
            L, *state, "tas.async.all", true,
            [](const auto &tasks) { return AreAllDone(tasks); })) {
        scheduler->YieldTicks(1);
        return lua_yieldk(L, 0, ctx, AllCont);
    }

    auto tasks = CollectStateTasks(L, *state, "tas.async.all", true);
    tas::lua::LuaYieldState<TaskListState>::Release(L, ctx);
    return PushAllOutcome(L, tasks);
}

static int AllRaw(lua_State *L) {
    auto tasks = CollectTasks(L, 1, "tas.async.all", true);
    if (AreAllDone(tasks)) {
        return PushAllOutcome(L, tasks);
    }
    LuaScheduler *scheduler = RequireTaskScheduler(L, tasks.empty() ? AsyncTaskHandle{} : tasks.front());
    if (!lua_isyieldable(L) || !scheduler->CanYieldCurrentThread()) {
        std::vector<AsyncTaskHandle>().swap(tasks);
        return YieldabilityError(L, "tas.async.all");
    }

    EnsureAllScheduled(L, tasks);
    // lua_yieldk longjmps (Lua is built as C), so destructors in this frame never run.
    std::vector<AsyncTaskHandle>().swap(tasks);
    const auto ctx = tas::lua::LuaYieldState<TaskListState>::Create(
        L, ReferenceTaskTable(L, 1));
    scheduler->YieldTicks(1);
    return lua_yieldk(L, 0, ctx, AllCont);
}

static int PushRaceOutcome(lua_State *L, const std::vector<AsyncTaskHandle> &tasks, const char *context) {
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

static int RaceCont(lua_State *L, int, lua_KContext ctx) {
    auto *state = tas::lua::LuaYieldState<TaskListState>::TryGet(L, ctx);
    if (!state) {
        tas::lua::LuaYieldState<TaskListState>::Release(L, ctx);
        return luaL_error(L, "tas.async.race continuation lost task list");
    }
    if (auto *scheduler = SchedulerWhilePending(
            L, *state, "tas.async.race", false,
            [](const auto &tasks) { return IsAnyDone(tasks); })) {
        scheduler->YieldTicks(1);
        return lua_yieldk(L, 0, ctx, RaceCont);
    }

    auto tasks = CollectStateTasks(L, *state, "tas.async.race", false);
    tas::lua::LuaYieldState<TaskListState>::Release(L, ctx);
    return PushRaceOutcome(L, tasks, "tas.async.race");
}

static int RaceRaw(lua_State *L) {
    auto tasks = CollectTasks(L, 1, "tas.async.race", false);
    if (IsAnyDone(tasks)) {
        return PushRaceOutcome(L, tasks, "tas.async.race");
    }
    LuaScheduler *scheduler = RequireTaskScheduler(L, tasks.front());
    if (!lua_isyieldable(L) || !scheduler->CanYieldCurrentThread()) {
        std::vector<AsyncTaskHandle>().swap(tasks);
        return YieldabilityError(L, "tas.async.race");
    }

    EnsureAllScheduled(L, tasks);
    // lua_yieldk longjmps (Lua is built as C), so destructors in this frame never run.
    std::vector<AsyncTaskHandle>().swap(tasks);
    const auto ctx = tas::lua::LuaYieldState<TaskListState>::Create(
        L, ReferenceTaskTable(L, 1));
    scheduler->YieldTicks(1);
    return lua_yieldk(L, 0, ctx, RaceCont);
}

static int PushAnyOutcome(lua_State *L, const std::vector<AsyncTaskHandle> &tasks) {
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
        return luaL_error(L, "tas.async.any: resumed before any task completed");
    }

    lua_pushboolean(L, 0);
    lua_createtable(L, 0, 3);
    lua_pushstring(L, "all_failed");
    lua_setfield(L, -2, "kind");
    lua_pushstring(L, "tas.async.any: all tasks failed or were cancelled");
    lua_setfield(L, -2, "message");
    MarkAsyncError(L);
    lua_createtable(L, static_cast<int>(errorIndexes.size()), 0);
    int outIndex = 1;
    for (size_t taskIndex : errorIndexes) {
        PushTaskErrorTable(L, *tasks[taskIndex], taskIndex, "tas.async.any");
        lua_rawseti(L, -2, outIndex++);
    }
    lua_setfield(L, -2, "errors");
    return 2;
}

static int AnyCont(lua_State *L, int, lua_KContext ctx) {
    auto *state = tas::lua::LuaYieldState<TaskListState>::TryGet(L, ctx);
    if (!state) {
        tas::lua::LuaYieldState<TaskListState>::Release(L, ctx);
        return luaL_error(L, "tas.async.any continuation lost task list");
    }
    if (auto *scheduler = SchedulerWhilePending(
            L, *state, "tas.async.any", false,
            [](const auto &tasks) { return IsAnyCompletedOrAllDone(tasks); })) {
        scheduler->YieldTicks(1);
        return lua_yieldk(L, 0, ctx, AnyCont);
    }

    auto tasks = CollectStateTasks(L, *state, "tas.async.any", false);
    tas::lua::LuaYieldState<TaskListState>::Release(L, ctx);
    return PushAnyOutcome(L, tasks);
}

static int AnyRaw(lua_State *L) {
    auto tasks = CollectTasks(L, 1, "tas.async.any", false);
    if (IsAnyCompletedOrAllDone(tasks)) {
        return PushAnyOutcome(L, tasks);
    }
    LuaScheduler *scheduler = RequireTaskScheduler(L, tasks.front());
    if (!lua_isyieldable(L) || !scheduler->CanYieldCurrentThread()) {
        std::vector<AsyncTaskHandle>().swap(tasks);
        return YieldabilityError(L, "tas.async.any");
    }

    EnsureAllScheduled(L, tasks);
    // lua_yieldk longjmps (Lua is built as C), so destructors in this frame never run.
    std::vector<AsyncTaskHandle>().swap(tasks);
    const auto ctx = tas::lua::LuaYieldState<TaskListState>::Create(
        L, ReferenceTaskTable(L, 1));
    scheduler->YieldTicks(1);
    return lua_yieldk(L, 0, ctx, AnyCont);
}

static int AwaitTicks(lua_State *L) {
    const int ticks = LuaApiCommon::CheckTicks(L, 1, "tas.await");
    RequireYieldableScheduler(L, "tas.await")->YieldTicks(ticks);
    return lua_yieldk(L, 0, 0, nullptr);
}

static int AwaitRaw(lua_State *L) {
    if (luaL_testudata(L, 1, kAsyncTaskMt)) {
        return AwaitTaskRaw(L);
    }
    if (lua_isinteger(L, 1)) {
        return AwaitTicks(L);
    }
    return LuaApiCommon::RaiseArg(L, "tas.await", 1, "AsyncTask or tick count expected");
}

static void SetContextFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context, LuaScheduler *scheduler) {
    lua_pushlightuserdata(L, context);
    lua_pushlightuserdata(L, scheduler);
    lua_pushcclosure(L, function, 2);
    lua_setfield(L, -2, name);
}

// Runs an install chunk with the `nargs` values on the stack as its arguments.
static void RunAsyncInstallChunk(lua_State *L, const char *chunk, const char *name, int nargs) {
    if (luaL_loadbuffer(L, chunk, std::strlen(chunk), name) != LUA_OK) {
        const char *message = lua_tostring(L, -1);
        std::string error = message ? message : "unknown Lua wrapper load error";
        lua_pop(L, 1 + nargs);
        throw std::runtime_error(std::string(name) + ": " + error);
    }
    lua_insert(L, -(nargs + 1));
    if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
        const char *message = lua_tostring(L, -1);
        std::string error = message ? message : "unknown Lua wrapper install error";
        lua_pop(L, 1);
        throw std::runtime_error(std::string(name) + ": " + error);
    }
}

static void PushAsyncClosure(lua_State *L, lua_CFunction function, ScriptContext *context, LuaScheduler *scheduler) {
    lua_pushlightuserdata(L, context);
    lua_pushlightuserdata(L, scheduler);
    lua_pushcclosure(L, function, 2);
}

static void InstallTaskAwaitWrapper(lua_State *L, ScriptContext *context, LuaScheduler *scheduler) {
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

    constexpr const char *chunk =
        "local methods, raw = ...\n"
        "methods.await = function(self)\n"
        "  local ok, value = raw(self)\n"
        "  if ok then return value end\n"
        "  error(value, 0)\n"
        "end\n";
    lua_pushvalue(L, -1);
    PushAsyncClosure(L, AwaitTaskRaw, context, scheduler);
    RunAsyncInstallChunk(L, chunk, "=BallanceTAS.AsyncTask.await", 2);
    lua_pop(L, 2);
}

static void InstallAsyncWrappers(lua_State *L, ScriptContext *context, LuaScheduler *scheduler) {
    constexpr const char *chunk =
        "local awaitRaw, allRaw, raceRaw, anyRaw = ...\n"
        "local tas = tas\n"
        "local async = tas.async\n"
        "local function unwrap(ok, value)\n"
        "  if ok then return value end\n"
        "  error(value, 0)\n"
        "end\n"
        "tas.await = function(value)\n"
        "  if type(value) == 'number' then return tas.wait_ticks(value) end\n"
        "  return unwrap(awaitRaw(value))\n"
        "end\n"
        "tas.async.all = function(tasks) return unwrap(allRaw(tasks)) end\n"
        "tas.async.race = function(tasks) return unwrap(raceRaw(tasks)) end\n"
        "tas.async.any = function(tasks) return unwrap(anyRaw(tasks)) end\n";
    PushAsyncClosure(L, AwaitRaw, context, scheduler);
    PushAsyncClosure(L, AllRaw, context, scheduler);
    PushAsyncClosure(L, RaceRaw, context, scheduler);
    PushAsyncClosure(L, AnyRaw, context, scheduler);
    RunAsyncInstallChunk(L, chunk, "=BallanceTAS.async", 4);
}

static void RegisterAsyncTable(lua_State *L, ScriptContext *context, LuaScheduler *scheduler) {
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
    // Deprecated: the same function objects as tas.wait_ticks / wait_event / wait_until.
    lua_getfield(L, -2, "wait_ticks");
    lua_setfield(L, -2, "delay");
    lua_getfield(L, -2, "wait_event");
    lua_setfield(L, -2, "wait_for_event");
    lua_getfield(L, -2, "wait_until");
    lua_setfield(L, -2, "wait_until");

    // tas.async(fn) is tas.async.create(fn).
    lua_newtable(L);
    lua_pushlightuserdata(L, context);
    lua_pushlightuserdata(L, scheduler);
    lua_pushcclosure(L, AsyncCall, 2);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);

    lua_setfield(L, -2, "async");
    InstallAsyncWrappers(L, context, scheduler);
    lua_pop(L, 1);

    // For the prelude's event dispatch (see LuaApi::kRunTaskNowKey).
    PushAsyncClosure(L, RunNow, context, scheduler);
    lua_setfield(L, LUA_REGISTRYINDEX, LuaApi::kRunTaskNowKey);
}

void LuaApi::RegisterAsyncApi(lua_State *state, ScriptContext *context, LuaScheduler *scheduler) {
    if (!state || (!context && !scheduler)) {
        throw std::runtime_error("LuaApi::RegisterAsyncApi requires a valid Lua state and ScriptContext or LuaScheduler");
    }

    tas::lua::LuaStackGuard guard(state);
    luaL_newmetatable(state, kAsyncErrorMt);
    lua_pushcfunction(state, AsyncErrorToString);
    lua_setfield(state, -2, "__tostring");
    lua_pop(state, 1);

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
