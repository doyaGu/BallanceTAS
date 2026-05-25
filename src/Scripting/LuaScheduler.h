#pragma once

#include <list>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "LuaRuntime/LuaFunction.h"
#include "LuaRuntime/LuaThread.h"
#include "LuaRuntime/LuaValue.h"
#include "ThreadOwnershipValidator.h"

class TASEngine;
class ScriptContext;
class AsyncTask;

class SchedulerTask {
public:
    virtual ~SchedulerTask() = default;
    virtual bool IsComplete() = 0;
};

class EventWaitTask;
class MessageResponseTask;

namespace detail {
    struct SchedulerThreadTask {
        std::shared_ptr<tas::lua::LuaThread> thread;
        std::shared_ptr<SchedulerTask> task;
    };
}

/**
 * @class LuaScheduler
 * @brief Manages Lua coroutine advancement without exposing raw Lua stack state.
 *
 * LuaScheduler is not thread-safe. Each ScriptContext owns one scheduler and
 * calls it from the game thread that advances that context.
 */
class LuaScheduler {
public:
    explicit LuaScheduler(TASEngine *engine, ScriptContext *context);
    ~LuaScheduler() = default;

    void StartCoroutine(tas::lua::LuaThread thread);
    void StartCoroutine(tas::lua::LuaFunction function);

    void AddCoroutineTask(tas::lua::LuaThread thread);
    void AddCoroutineTask(tas::lua::LuaFunction function);

    std::shared_ptr<tas::lua::LuaThread> StartCoroutineAndTrack(tas::lua::LuaThread thread);
    std::shared_ptr<tas::lua::LuaThread> StartCoroutineAndTrack(tas::lua::LuaFunction function);

    void Tick();
    void Clear();
    void Pause();
    void Resume();

    bool IsPaused() const { return m_IsPaused; }
    bool IsRunning() const;
    bool CanYieldCurrentThread() const;
    size_t GetTaskCount() const;

    void YieldTicks(int ticks);
    void YieldUntil(tas::lua::LuaFunction predicate);
    void YieldCoroutines(const std::vector<std::shared_ptr<tas::lua::LuaThread>> &coroutines);
    void YieldRace(const std::vector<std::shared_ptr<tas::lua::LuaThread>> &coroutines);
    void YieldWaitForEvent(const std::string &eventName);
    tas::lua::LuaValue YieldWaitForMessageResponse(const std::string &correlationId, int timeoutMs);
    void StartAsyncTask(const std::shared_ptr<AsyncTask> &task);

    void StartRepeatFor(tas::lua::LuaFunction task, int ticks);
    void StartRepeatUntil(tas::lua::LuaFunction task, tas::lua::LuaFunction condition);
    void StartRepeatWhile(tas::lua::LuaFunction task, tas::lua::LuaFunction condition);

    void StartDelay(tas::lua::LuaFunction task, int delayTicks);
    void StartTimeout(tas::lua::LuaFunction task, int timeoutTicks);
    void StartDebounce(tas::lua::LuaFunction task, int debounceTicks);

    void StartSequence(std::vector<tas::lua::LuaFunction> tasks);
    void StartRetry(tas::lua::LuaFunction task, int maxAttempts);

    void StartParallel(std::vector<tas::lua::LuaFunction> functions);
    void StartParallel(const std::vector<std::shared_ptr<tas::lua::LuaThread>> &coroutines);

private:
    void Yield(std::shared_ptr<SchedulerTask> task);
    void ResumeThread(const std::shared_ptr<tas::lua::LuaThread> &thread);
    std::shared_ptr<tas::lua::LuaThread> CreateThreadFromFunction(tas::lua::LuaFunction function);

    TASEngine *m_Engine;
    ScriptContext *m_Context;
    std::shared_ptr<tas::lua::LuaThread> m_CurrentThread;
    std::list<detail::SchedulerThreadTask> m_Tasks;
    std::list<std::shared_ptr<SchedulerTask>> m_BackgroundTasks;
    std::stack<std::shared_ptr<tas::lua::LuaThread>> m_ThreadStack;
    std::unordered_map<const tas::lua::LuaThread *, std::shared_ptr<AsyncTask>> m_AsyncOwners;
    bool m_IsPaused = false;

    mutable ThreadOwnershipValidator m_ThreadValidator{"LuaScheduler"};
};
