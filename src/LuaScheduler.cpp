#include "LuaScheduler.h"

#include <stdexcept>
#include <functional>
#include <utility>

#include "AsyncTask.h"
#include "EventManager.h"
#include "Logger.h"
#include "MessageBus.h"
#include "ScriptContext.h"
#include "TASEngine.h"

using tas::lua::LuaFunction;
using tas::lua::LuaThread;
using tas::lua::LuaThreadStatus;
using tas::lua::LuaValue;

static std::shared_ptr<LuaFunction> MakeFunction(LuaFunction function) {
    if (!function.IsValid()) {
        return {};
    }
    return std::make_shared<LuaFunction>(std::move(function));
}

static bool CallFunctionNoResults(const std::shared_ptr<LuaFunction> &function, const char *context) {
    if (!function || !function->IsValid()) {
        return false;
    }

    auto result = function->Call(0, 0);
    if (result.IsError()) {
        Log::Error("%s: %s", context, result.GetError().message.c_str());
        return false;
    }
    return true;
}

static bool CallFunctionBool(const std::shared_ptr<LuaFunction> &function, bool defaultValue, const char *context) {
    if (!function || !function->IsValid()) {
        return defaultValue;
    }

    lua_State *state = function->State();
    const int top = lua_gettop(state);
    auto result = function->Call(0, 1);
    if (result.IsError()) {
        lua_settop(state, top);
        Log::Error("%s: %s", context, result.GetError().message.c_str());
        return defaultValue;
    }

    const bool value = lua_toboolean(state, -1) != 0;
    lua_settop(state, top);
    return value;
}

class ImmediateTask final : public SchedulerTask {
public:
    bool IsComplete() override { return true; }
};

class TickWaitTask final : public SchedulerTask {
public:
    explicit TickWaitTask(int ticks) : m_RemainingTicks(ticks) {}

    bool IsComplete() override {
        --m_RemainingTicks;
        return m_RemainingTicks <= 0;
    }

private:
    int m_RemainingTicks;
};

class PredicateWaitTask final : public SchedulerTask {
public:
    explicit PredicateWaitTask(LuaFunction predicate)
        : m_Predicate(MakeFunction(std::move(predicate))) {}

    bool IsComplete() override {
        return CallFunctionBool(m_Predicate, true, "PredicateWaitTask");
    }

private:
    std::shared_ptr<LuaFunction> m_Predicate;
};

class CoroutineWaitTask final : public SchedulerTask {
public:
    explicit CoroutineWaitTask(std::vector<std::shared_ptr<LuaThread>> coroutines)
        : m_Coroutines(std::move(coroutines)) {}

    bool IsComplete() override {
        for (const auto &thread : m_Coroutines) {
            if (!thread || !thread->IsValid()) {
                continue;
            }
            const LuaThreadStatus status = thread->Status();
            if (status == LuaThreadStatus::Runnable || status == LuaThreadStatus::Yielded) {
                return false;
            }
        }
        return true;
    }

private:
    std::vector<std::shared_ptr<LuaThread>> m_Coroutines;
};

class RaceTask final : public SchedulerTask {
public:
    explicit RaceTask(std::vector<std::shared_ptr<LuaThread>> coroutines)
        : m_Coroutines(std::move(coroutines)) {}

    bool IsComplete() override {
        for (const auto &thread : m_Coroutines) {
            if (!thread || !thread->IsValid()) {
                return true;
            }
            const LuaThreadStatus status = thread->Status();
            if (status == LuaThreadStatus::Dead || status == LuaThreadStatus::Error) {
                return true;
            }
        }
        return false;
    }

private:
    std::vector<std::shared_ptr<LuaThread>> m_Coroutines;
};

class RepeatTask : public SchedulerTask {
public:
    explicit RepeatTask(LuaFunction task)
        : m_Task(MakeFunction(std::move(task))) {}

protected:
    bool ExecuteTask(const char *context) {
        return CallFunctionNoResults(m_Task, context);
    }

    std::shared_ptr<LuaFunction> m_Task;
};

class RepeatForTicksTask final : public RepeatTask {
public:
    RepeatForTicksTask(LuaFunction task, int ticks)
        : RepeatTask(std::move(task)), m_RemainingTicks(ticks) {}

    bool IsComplete() override {
        if (m_RemainingTicks <= 0) {
            return true;
        }

        ExecuteTask("RepeatForTicksTask");
        --m_RemainingTicks;
        return m_RemainingTicks <= 0;
    }

private:
    int m_RemainingTicks;
};

class RepeatUntilTask final : public RepeatTask {
public:
    RepeatUntilTask(LuaFunction task, LuaFunction condition)
        : RepeatTask(std::move(task)), m_Condition(MakeFunction(std::move(condition))) {}

    bool IsComplete() override {
        if (CallFunctionBool(m_Condition, true, "RepeatUntilTask.condition")) {
            return true;
        }
        ExecuteTask("RepeatUntilTask.task");
        return false;
    }

private:
    std::shared_ptr<LuaFunction> m_Condition;
};

class RepeatWhileTask final : public RepeatTask {
public:
    RepeatWhileTask(LuaFunction task, LuaFunction condition)
        : RepeatTask(std::move(task)), m_Condition(MakeFunction(std::move(condition))) {}

    bool IsComplete() override {
        if (!CallFunctionBool(m_Condition, false, "RepeatWhileTask.condition")) {
            return true;
        }
        ExecuteTask("RepeatWhileTask.task");
        return false;
    }

private:
    std::shared_ptr<LuaFunction> m_Condition;
};

class DelayTask final : public SchedulerTask {
public:
    DelayTask(LuaFunction task, int delayTicks)
        : m_Task(MakeFunction(std::move(task))), m_DelayTicks(delayTicks) {}

    bool IsComplete() override {
        if (m_DelayTicks > 0) {
            --m_DelayTicks;
            return false;
        }

        if (!m_TaskExecuted) {
            CallFunctionNoResults(m_Task, "DelayTask");
            m_TaskExecuted = true;
        }
        return true;
    }

private:
    std::shared_ptr<LuaFunction> m_Task;
    int m_DelayTicks;
    bool m_TaskExecuted = false;
};

class TimeoutTask final : public SchedulerTask {
public:
    TimeoutTask(LuaFunction task, int timeoutTicks)
        : m_Task(MakeFunction(std::move(task))), m_TimeoutTicks(timeoutTicks) {}

    bool IsComplete() override {
        if (m_TaskComplete || m_TimeoutTicks <= 0) {
            return true;
        }

        --m_TimeoutTicks;
        m_TaskComplete = CallFunctionBool(m_Task, false, "TimeoutTask");
        return m_TaskComplete || m_TimeoutTicks <= 0;
    }

private:
    std::shared_ptr<LuaFunction> m_Task;
    int m_TimeoutTicks;
    bool m_TaskComplete = false;
};

class SequenceTask final : public SchedulerTask {
public:
    explicit SequenceTask(std::vector<LuaFunction> tasks) {
        m_Tasks.reserve(tasks.size());
        for (auto &task : tasks) {
            m_Tasks.push_back(MakeFunction(std::move(task)));
        }
    }

    bool IsComplete() override {
        if (m_CurrentIndex >= m_Tasks.size()) {
            return true;
        }

        CallFunctionNoResults(m_Tasks[m_CurrentIndex], "SequenceTask");
        ++m_CurrentIndex;
        return m_CurrentIndex >= m_Tasks.size();
    }

private:
    std::vector<std::shared_ptr<LuaFunction>> m_Tasks;
    size_t m_CurrentIndex = 0;
};

class RetryTask final : public SchedulerTask {
public:
    RetryTask(LuaFunction task, int maxAttempts)
        : m_Task(MakeFunction(std::move(task))), m_MaxAttempts(maxAttempts) {}

    bool IsComplete() override {
        if (m_CurrentAttempt >= m_MaxAttempts) {
            return true;
        }

        ++m_CurrentAttempt;
        if (CallFunctionBool(m_Task, false, "RetryTask")) {
            return true;
        }
        return m_CurrentAttempt >= m_MaxAttempts;
    }

private:
    std::shared_ptr<LuaFunction> m_Task;
    int m_MaxAttempts;
    int m_CurrentAttempt = 0;
};

class DebounceTask final : public SchedulerTask {
public:
    DebounceTask(LuaFunction task, int debounceTicks)
        : m_Task(MakeFunction(std::move(task))),
          m_DebounceTicks(debounceTicks),
          m_RemainingTicks(debounceTicks) {}

    bool IsComplete() override {
        if (m_TaskExecuted) {
            return true;
        }

        --m_RemainingTicks;
        if (m_RemainingTicks <= 0) {
            CallFunctionNoResults(m_Task, "DebounceTask");
            m_TaskExecuted = true;
            return true;
        }
        return false;
    }

    void Reset() {
        m_RemainingTicks = m_DebounceTicks;
        m_TaskExecuted = false;
    }

private:
    std::shared_ptr<LuaFunction> m_Task;
    int m_DebounceTicks;
    int m_RemainingTicks;
    bool m_TaskExecuted = false;
};

class EventWaitTask final : public SchedulerTask {
public:
    EventWaitTask(std::string eventName, TASEngine *engine, EventManager *eventManager);
    ~EventWaitTask() override;

    bool IsComplete() override;

private:
    std::string m_EventName;
    TASEngine *m_Engine;
    EventManager *m_EventManager;
    bool m_EventReceived = false;
    uint64_t m_ListenerId = 0;
};

class MessageResponseTask final : public SchedulerTask {
public:
    MessageResponseTask(std::string correlationId, TASEngine *engine, int timeoutTicks);

    bool IsComplete() override;
    LuaValue GetResponse() const;

private:
    std::string m_CorrelationId;
    TASEngine *m_Engine;
    int m_TimeoutTicks;
    bool m_ResponseReceived = false;
    LuaValue m_ResponseData;
};

EventWaitTask::EventWaitTask(std::string eventName, TASEngine *engine, EventManager *eventManager)
    : m_EventName(std::move(eventName)), m_Engine(engine), m_EventManager(eventManager) {
    if (!m_EventManager) {
        Log::Error("EventWaitTask: Event manager unavailable for event '%s'", m_EventName.c_str());
        m_EventReceived = true;
        return;
    }

    std::function<void()> callback = [this]() {
        m_EventReceived = true;
    };
    m_ListenerId = m_EventManager->RegisterListener(m_EventName, callback, true);

    if (m_ListenerId == EventManager::kInvalidListenerId && m_Engine) {
        Log::Error("EventWaitTask: Failed to register listener for event '%s'", m_EventName.c_str());
        m_EventReceived = true;
    }
}

EventWaitTask::~EventWaitTask() {
    if (m_EventManager && m_ListenerId != EventManager::kInvalidListenerId) {
        m_EventManager->UnregisterListener(m_EventName, m_ListenerId);
        m_ListenerId = EventManager::kInvalidListenerId;
    }
}

bool EventWaitTask::IsComplete() {
    return m_EventReceived;
}

MessageResponseTask::MessageResponseTask(std::string correlationId, TASEngine *engine, int timeoutTicks)
    : m_CorrelationId(std::move(correlationId)),
      m_Engine(engine),
      m_TimeoutTicks(timeoutTicks) {}

bool MessageResponseTask::IsComplete() {
    if (m_TimeoutTicks <= 0) {
        if (m_Engine) {
            Log::Warn("MessageResponseTask: Timeout waiting for response (correlation_id: %s)",
                      m_CorrelationId.c_str());
        }
        return true;
    }

    --m_TimeoutTicks;
    return false;
}

LuaValue MessageResponseTask::GetResponse() const {
    return m_ResponseReceived ? m_ResponseData : LuaValue();
}

LuaScheduler::LuaScheduler(TASEngine *engine, ScriptContext *context)
    : m_Engine(engine), m_Context(context) {
    if (!m_Engine) {
        throw std::runtime_error("LuaScheduler requires a valid TASEngine instance");
    }
    if (!m_Context) {
        throw std::runtime_error("LuaScheduler requires a valid ScriptContext");
    }
}

void LuaScheduler::StartCoroutine(LuaThread thread) {
    auto tracked = std::make_shared<LuaThread>(std::move(thread));
    ResumeThread(tracked);
}

void LuaScheduler::StartCoroutine(LuaFunction function) {
    auto thread = CreateThreadFromFunction(std::move(function));
    if (thread) {
        ResumeThread(thread);
    }
}

void LuaScheduler::AddCoroutineTask(LuaThread thread) {
    m_ThreadValidator.AssertOwnership();

    if (!thread.IsValid()) {
        Log::Error("AddCoroutineTask: invalid Lua thread provided");
        return;
    }

    m_Tasks.push_back({std::make_shared<LuaThread>(std::move(thread)), std::make_shared<ImmediateTask>()});
}

void LuaScheduler::AddCoroutineTask(LuaFunction function) {
    m_ThreadValidator.AssertOwnership();

    auto thread = CreateThreadFromFunction(std::move(function));
    if (!thread) {
        Log::Error("AddCoroutineTask: invalid Lua function provided");
        return;
    }

    m_Tasks.push_back({std::move(thread), std::make_shared<ImmediateTask>()});
}

std::shared_ptr<LuaThread> LuaScheduler::StartCoroutineAndTrack(LuaThread thread) {
    m_ThreadValidator.AssertOwnership();

    if (!thread.IsValid()) {
        Log::Error("StartCoroutineAndTrack: invalid Lua thread provided");
        return {};
    }

    auto tracked = std::make_shared<LuaThread>(std::move(thread));
    m_Tasks.push_back({tracked, std::make_shared<ImmediateTask>()});
    return tracked;
}

std::shared_ptr<LuaThread> LuaScheduler::StartCoroutineAndTrack(LuaFunction function) {
    m_ThreadValidator.AssertOwnership();

    auto thread = CreateThreadFromFunction(std::move(function));
    if (!thread) {
        Log::Error("StartCoroutineAndTrack: invalid Lua function provided");
        return {};
    }

    m_Tasks.push_back({thread, std::make_shared<ImmediateTask>()});
    return thread;
}

void LuaScheduler::Tick() {
    m_ThreadValidator.AssertOwnership();

    if (m_IsPaused) {
        return;
    }

    size_t pendingAtTickStart = m_Tasks.size();
    for (auto it = m_Tasks.begin(); it != m_Tasks.end() && pendingAtTickStart > 0; --pendingAtTickStart) {
        if (!it->thread || !it->thread->IsValid()) {
            if (it->thread) {
                auto ownerIt = m_AsyncOwners.find(it->thread.get());
                if (ownerIt != m_AsyncOwners.end()) {
                    if (auto owner = ownerIt->second; owner && !owner->IsDone()) {
                        owner->SetError("async task coroutine is invalid");
                    }
                    m_AsyncOwners.erase(ownerIt);
                }
            }
            it = m_Tasks.erase(it);
            continue;
        }

        auto ownerIt = m_AsyncOwners.find(it->thread.get());
        if (ownerIt != m_AsyncOwners.end()) {
            auto owner = ownerIt->second;
            if (owner && owner->IsCancelled()) {
                m_AsyncOwners.erase(ownerIt);
                it = m_Tasks.erase(it);
                continue;
            }
        }

        const LuaThreadStatus status = it->thread->Status();
        if (status == LuaThreadStatus::Dead || status == LuaThreadStatus::Error) {
            auto ownerIt = m_AsyncOwners.find(it->thread.get());
            if (ownerIt != m_AsyncOwners.end()) {
                if (auto owner = ownerIt->second; owner && !owner->IsDone()) {
                    owner->SetError(status == LuaThreadStatus::Error
                                        ? "async task coroutine is in error state"
                                        : "async task coroutine ended before completion was recorded");
                }
                m_AsyncOwners.erase(ownerIt);
            }
            it = m_Tasks.erase(it);
            continue;
        }

        if (!it->task->IsComplete()) {
            ++it;
            continue;
        }

        auto thread = it->thread;
        it = m_Tasks.erase(it);
        ResumeThread(thread);
    }

    for (auto it = m_BackgroundTasks.begin(); it != m_BackgroundTasks.end();) {
        if ((*it)->IsComplete()) {
            it = m_BackgroundTasks.erase(it);
        } else {
            ++it;
        }
    }
}

void LuaScheduler::Clear() {
    m_ThreadValidator.AssertOwnership();

    m_Tasks.clear();
    m_BackgroundTasks.clear();
    m_AsyncOwners.clear();
    m_CurrentThread.reset();
    m_IsPaused = false;
    while (!m_ThreadStack.empty()) {
        m_ThreadStack.pop();
    }
}

void LuaScheduler::Pause() {
    m_ThreadValidator.AssertOwnership();
    m_IsPaused = true;
}

void LuaScheduler::Resume() {
    m_ThreadValidator.AssertOwnership();
    m_IsPaused = false;
}

bool LuaScheduler::IsRunning() const {
    return !m_Tasks.empty() || !m_BackgroundTasks.empty();
}

bool LuaScheduler::CanYieldCurrentThread() const {
    return m_CurrentThread && m_CurrentThread->IsValid();
}

size_t LuaScheduler::GetTaskCount() const {
    return m_Tasks.size() + m_BackgroundTasks.size();
}

void LuaScheduler::YieldTicks(int ticks) {
    m_ThreadValidator.AssertOwnership();

    if (ticks <= 0) {
        Log::Error("YieldTicks: tick count must be positive");
        return;
    }
    if (!m_CurrentThread) {
        Log::Error("YieldTicks called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<TickWaitTask>(ticks));
}

void LuaScheduler::YieldUntil(LuaFunction predicate) {
    m_ThreadValidator.AssertOwnership();

    if (!predicate.IsValid()) {
        Log::Error("YieldUntil: invalid predicate function");
        return;
    }
    if (!m_CurrentThread) {
        Log::Error("YieldUntil called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<PredicateWaitTask>(std::move(predicate)));
}

void LuaScheduler::YieldCoroutines(const std::vector<std::shared_ptr<LuaThread>> &coroutines) {
    m_ThreadValidator.AssertOwnership();

    if (coroutines.empty()) {
        Log::Error("YieldCoroutines: no coroutines to wait for");
        return;
    }
    if (!m_CurrentThread) {
        Log::Error("YieldCoroutines called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<CoroutineWaitTask>(coroutines));
}

void LuaScheduler::YieldRace(const std::vector<std::shared_ptr<LuaThread>> &coroutines) {
    m_ThreadValidator.AssertOwnership();

    if (coroutines.empty()) {
        Log::Error("YieldRace: no coroutines to wait for");
        return;
    }
    if (!m_CurrentThread) {
        Log::Error("YieldRace called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<RaceTask>(coroutines));
}

void LuaScheduler::YieldWaitForEvent(const std::string &eventName) {
    m_ThreadValidator.AssertOwnership();

    if (eventName.empty()) {
        Log::Error("YieldWaitForEvent: event name cannot be empty");
        return;
    }
    if (!m_CurrentThread) {
        Log::Error("YieldWaitForEvent called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<EventWaitTask>(eventName, m_Engine, m_Context ? m_Context->GetEventManager() : nullptr));
}

void LuaScheduler::StartAsyncTask(const std::shared_ptr<AsyncTask> &task) {
    m_ThreadValidator.AssertOwnership();

    if (!task) {
        Log::Error("StartAsyncTask: invalid async task");
        return;
    }
    if (task->IsDone()) {
        return;
    }
    if (task->IsScheduled()) {
        return;
    }

    auto thread = task->GetThread();
    if (!thread || !thread->IsValid()) {
        task->SetError("async task coroutine is invalid");
        return;
    }

    task->Start();
    task->MarkScheduled();
    m_AsyncOwners[thread.get()] = task;
    m_Tasks.push_back({std::move(thread), std::make_shared<ImmediateTask>()});
}

LuaValue LuaScheduler::YieldWaitForMessageResponse(const std::string &correlationId, int timeoutMs) {
    m_ThreadValidator.AssertOwnership();

    if (correlationId.empty()) {
        Log::Error("YieldWaitForMessageResponse: correlation_id cannot be empty");
        return LuaValue();
    }
    if (!m_CurrentThread) {
        Log::Error("YieldWaitForMessageResponse called outside of coroutine context");
        return LuaValue();
    }

    constexpr int kTicksPerSecond = 60;
    int timeoutTicks = (timeoutMs * kTicksPerSecond) / 1000;
    if (timeoutTicks <= 0) {
        timeoutTicks = 300;
    }

    auto task = std::make_shared<MessageResponseTask>(correlationId, m_Engine, timeoutTicks);
    Yield(task);
    return task->GetResponse();
}

void LuaScheduler::StartRepeatFor(LuaFunction task, int ticks) {
    m_ThreadValidator.AssertOwnership();

    if (!task.IsValid()) {
        Log::Error("StartRepeatFor: invalid task function");
        return;
    }
    if (ticks <= 0) {
        Log::Error("StartRepeatFor: tick count must be positive");
        return;
    }

    m_BackgroundTasks.push_back(std::make_shared<RepeatForTicksTask>(std::move(task), ticks));
}

void LuaScheduler::StartRepeatUntil(LuaFunction task, LuaFunction condition) {
    m_ThreadValidator.AssertOwnership();

    if (!task.IsValid() || !condition.IsValid()) {
        Log::Error("StartRepeatUntil: invalid task or condition function");
        return;
    }

    m_BackgroundTasks.push_back(std::make_shared<RepeatUntilTask>(std::move(task), std::move(condition)));
}

void LuaScheduler::StartRepeatWhile(LuaFunction task, LuaFunction condition) {
    m_ThreadValidator.AssertOwnership();

    if (!task.IsValid() || !condition.IsValid()) {
        Log::Error("StartRepeatWhile: invalid task or condition function");
        return;
    }

    m_BackgroundTasks.push_back(std::make_shared<RepeatWhileTask>(std::move(task), std::move(condition)));
}

void LuaScheduler::StartDelay(LuaFunction task, int delayTicks) {
    m_ThreadValidator.AssertOwnership();

    if (!task.IsValid()) {
        Log::Error("StartDelay: invalid task function");
        return;
    }
    if (delayTicks < 0) {
        Log::Error("StartDelay: delay ticks cannot be negative");
        return;
    }

    m_BackgroundTasks.push_back(std::make_shared<DelayTask>(std::move(task), delayTicks));
}

void LuaScheduler::StartTimeout(LuaFunction task, int timeoutTicks) {
    m_ThreadValidator.AssertOwnership();

    if (!task.IsValid()) {
        Log::Error("StartTimeout: invalid task function");
        return;
    }
    if (timeoutTicks <= 0) {
        Log::Error("StartTimeout: timeout must be positive");
        return;
    }

    m_BackgroundTasks.push_back(std::make_shared<TimeoutTask>(std::move(task), timeoutTicks));
}

void LuaScheduler::StartDebounce(LuaFunction task, int debounceTicks) {
    m_ThreadValidator.AssertOwnership();

    if (!task.IsValid()) {
        Log::Error("StartDebounce: invalid task function");
        return;
    }
    if (debounceTicks <= 0) {
        Log::Error("StartDebounce: debounce ticks must be positive");
        return;
    }

    m_BackgroundTasks.push_back(std::make_shared<DebounceTask>(std::move(task), debounceTicks));
}

void LuaScheduler::StartSequence(std::vector<LuaFunction> tasks) {
    m_ThreadValidator.AssertOwnership();

    if (tasks.empty()) {
        Log::Error("StartSequence: no tasks provided");
        return;
    }

    m_BackgroundTasks.push_back(std::make_shared<SequenceTask>(std::move(tasks)));
}

void LuaScheduler::StartRetry(LuaFunction task, int maxAttempts) {
    m_ThreadValidator.AssertOwnership();

    if (!task.IsValid()) {
        Log::Error("StartRetry: invalid task function");
        return;
    }
    if (maxAttempts <= 0) {
        Log::Error("StartRetry: max attempts must be positive");
        return;
    }

    m_BackgroundTasks.push_back(std::make_shared<RetryTask>(std::move(task), maxAttempts));
}

void LuaScheduler::StartParallel(std::vector<LuaFunction> functions) {
    m_ThreadValidator.AssertOwnership();

    for (auto &function : functions) {
        AddCoroutineTask(std::move(function));
    }
}

void LuaScheduler::StartParallel(const std::vector<std::shared_ptr<LuaThread>> &coroutines) {
    m_ThreadValidator.AssertOwnership();

    for (const auto &coroutine : coroutines) {
        if (coroutine && coroutine->IsValid()) {
            m_Tasks.push_back({coroutine, std::make_shared<ImmediateTask>()});
        }
    }
}

void LuaScheduler::Yield(std::shared_ptr<SchedulerTask> task) {
    m_Tasks.push_back({m_CurrentThread, std::move(task)});
}

void LuaScheduler::ResumeThread(const std::shared_ptr<LuaThread> &thread) {
    m_ThreadValidator.AssertOwnership();

    if (!thread || !thread->IsValid()) {
        Log::Error("LuaScheduler: invalid Lua thread");
        return;
    }

    m_ThreadStack.push(thread);
    m_CurrentThread = thread;

    auto result = thread->Resume();
    auto ownerIt = m_AsyncOwners.find(thread.get());
    std::shared_ptr<AsyncTask> asyncOwner;
    if (ownerIt != m_AsyncOwners.end()) {
        asyncOwner = ownerIt->second;
    }
    if (result.IsError()) {
        Log::Error("Coroutine error: %s", result.GetError().message.c_str());
        if (asyncOwner) {
            asyncOwner->SetError(result.GetError().message);
        }
    } else {
        const LuaThreadStatus status = thread->Status();
        if (asyncOwner && status == LuaThreadStatus::Dead) {
            LuaValue resultValue;
            if (result.Unwrap() > 0) {
                auto value = LuaValue::FromStack(thread->State(), -1);
                if (value.IsOk()) {
                    resultValue = value.Unwrap();
                }
            }
            asyncOwner->SetResult(std::move(resultValue));
        }
        if (status == LuaThreadStatus::Dead) {
            lua_settop(thread->State(), 0);
        }
    }
    if (asyncOwner && asyncOwner->IsDone()) {
        m_AsyncOwners.erase(thread.get());
    }

    m_ThreadStack.pop();
    m_CurrentThread = m_ThreadStack.empty() ? nullptr : m_ThreadStack.top();
}

std::shared_ptr<LuaThread> LuaScheduler::CreateThreadFromFunction(LuaFunction function) {
    if (!function.IsValid()) {
        return {};
    }

    lua_State *state = function.State();
    const int top = lua_gettop(state);
    function.Push();
    LuaThread thread = LuaThread::CreateFromFunction(state, -1);
    lua_settop(state, top);

    if (!thread.IsValid()) {
        return {};
    }
    return std::make_shared<LuaThread>(std::move(thread));
}
