#include "LuaScheduler.h"

#include <utility>

#include "EventManager.h"
#include "TASEngine.h"

EventWaitTask::EventWaitTask(std::string eventName, TASEngine *engine)
    : m_EventName(std::move(eventName)), m_Engine(engine), m_EventReceived(false) {
    // Register as listener for the event
    if (m_Engine && m_Engine->GetEventManager()) {
        std::function callback = [this]() {
            m_EventReceived = true;
        };
        m_Engine->GetEventManager()->RegisterListener(m_EventName, callback);
    }
}

LuaScheduler::LuaScheduler(TASEngine *engine)
    : m_Engine(engine), m_CurrentThread(nullptr) {}

sol::state &LuaScheduler::GetLuaState() const {
    return m_Engine->GetLuaState();
}

void LuaScheduler::StartCoroutine(const sol::coroutine &co) {
    // Create a new thread context for the coroutine
    auto thread = std::make_shared<detail::SchedulerCothread>(GetLuaState(), co);

    // Push it onto the stack to set the current execution context
    m_ThreadStack.push(thread);
    m_CurrentThread = thread;

    // Start the coroutine
    auto result = m_CurrentThread->coroutine();

    // The coroutine has either finished or yielded.
    // If it did NOT yield, its execution is over, so we can pop it from the stack.
    // If it DID yield, a task was already created by a YieldXxx function,
    // and it remains on the stack as part of the execution context until it's done.
    if (m_CurrentThread->coroutine.status() != sol::call_status::yielded) {
        m_ThreadStack.pop();
    }

    // Reset current thread pointer to the top of the stack
    if (m_ThreadStack.empty()) {
        m_CurrentThread = nullptr;
    } else {
        m_CurrentThread = m_ThreadStack.top();
    }

    // Handle any errors
    if (!result.valid()) {
        sol::error err = result;
        m_Engine->GetLogger()->Error("Coroutine error: %s", err.what());
    }
}

void LuaScheduler::StartCoroutine(const sol::function &func) {
    StartCoroutine(sol::coroutine(func));
}

void LuaScheduler::AddCoroutineTask(const sol::coroutine &co) {
    // Create new thread for the coroutine
    auto thread = std::make_shared<detail::SchedulerCothread>(GetLuaState(), co);

    // Create an immediate task that will cause the coroutine to start on next tick
    auto task = std::make_shared<ImmediateTask>();

    // Add to task list
    detail::SchedulerThreadTask threadTask;
    threadTask.thread = thread;
    threadTask.task = task;
    m_Tasks.push_back(threadTask);
}

void LuaScheduler::AddCoroutineTask(const sol::function &func) {
    AddCoroutineTask(sol::coroutine(func));
}

void LuaScheduler::Tick() {
    // Process all pending tasks
    for (auto i = m_Tasks.begin(); i != m_Tasks.end();) {
        // If the thread is dead, remove it
        if (i->thread->coroutine.status() != sol::call_status::yielded) {
            i = m_Tasks.erase(i);
            continue;
        }

        // Is this task complete?
        if (i->task->IsComplete()) {
            // Get the thread task
            detail::SchedulerThreadTask thread_task = *i;
            // Remove it from the pending list
            i = m_Tasks.erase(i);

            // Set the current thread
            m_ThreadStack.push(thread_task.thread);
            m_CurrentThread = thread_task.thread;

            // Resume the thread
            auto result = thread_task.thread->coroutine();

            // Reset current thread
            m_ThreadStack.pop();
            if (m_ThreadStack.empty()) {
                m_CurrentThread = nullptr;
            } else {
                m_CurrentThread = m_ThreadStack.top();
            }

            // Handle any errors
            if (!result.valid()) {
                sol::error err = result;
                m_Engine->GetLogger()->Error("Coroutine resume error: %s", err.what());
            }
        } else {
            ++i;
        }
    }
}

void LuaScheduler::Clear() {
    m_Tasks.clear();
    m_CurrentThread = nullptr;
    // Clear the thread stack
    while (!m_ThreadStack.empty()) {
        m_ThreadStack.pop();
    }
}

bool LuaScheduler::IsRunning() const {
    return !m_Tasks.empty();
}

size_t LuaScheduler::GetTaskCount() const {
    return m_Tasks.size();
}

void LuaScheduler::YieldTicks(int ticks) {
    if (ticks <= 0) {
        m_Engine->GetLogger()->Error("YieldTicks: tick count must be positive");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldTicks called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<TickWaitTask>(ticks));
}

void LuaScheduler::YieldUntil(const sol::function &predicate) {
    if (!predicate.valid()) {
        m_Engine->GetLogger()->Error("YieldUntil: invalid predicate function");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldUntil called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<PredicateWaitTask>(predicate));
}

void LuaScheduler::YieldCoroutines(const std::vector<sol::coroutine> &coroutines) {
    if (coroutines.empty()) {
        m_Engine->GetLogger()->Error("YieldCoroutines: no coroutines to wait for");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldCoroutines called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<CoroutineWaitTask>(coroutines));
}

void LuaScheduler::YieldRepeatFor(const sol::function &task, int ticks) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("YieldRepeatFor: invalid task function");
        return;
    }

    if (ticks <= 0) {
        m_Engine->GetLogger()->Error("YieldRepeatFor: tick count must be positive");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldRepeatFor called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<RepeatForTicksTask>(task, ticks));
}

void LuaScheduler::YieldRepeatUntil(const sol::function &task, const sol::function &condition) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("YieldRepeatUntil: invalid task function");
        return;
    }

    if (!condition.valid()) {
        m_Engine->GetLogger()->Error("YieldRepeatUntil: invalid condition function");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldRepeatUntil called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<RepeatUntilTask>(task, condition));
}

void LuaScheduler::YieldRepeatWhile(const sol::function &task, const sol::function &condition) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("YieldRepeatWhile: invalid task function");
        return;
    }

    if (!condition.valid()) {
        m_Engine->GetLogger()->Error("YieldRepeatWhile: invalid condition function");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldRepeatWhile called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<RepeatWhileTask>(task, condition));
}

void LuaScheduler::YieldRepeatCount(const sol::function &task, int count) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("YieldRepeatCount: invalid task function");
        return;
    }

    if (count <= 0) {
        m_Engine->GetLogger()->Error("YieldRepeatCount: count must be positive");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldRepeatCount called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<RepeatCountTask>(task, count));
}

void LuaScheduler::YieldDelay(const sol::function &task, int delay_ticks) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("YieldDelay: invalid task function");
        return;
    }

    if (delay_ticks < 0) {
        m_Engine->GetLogger()->Error("YieldDelay: delay ticks cannot be negative");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldDelay called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<DelayTask>(task, delay_ticks));
}

void LuaScheduler::YieldTimeout(const sol::function &task, int timeout_ticks) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("YieldTimeout: invalid task function");
        return;
    }

    if (timeout_ticks <= 0) {
        m_Engine->GetLogger()->Error("YieldTimeout: timeout must be positive");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldTimeout called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<TimeoutTask>(task, timeout_ticks));
}

void LuaScheduler::YieldSequence(const std::vector<sol::function> &tasks) {
    if (tasks.empty()) {
        m_Engine->GetLogger()->Error("YieldSequence: no tasks provided");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldSequence called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<SequenceTask>(tasks));
}

void LuaScheduler::YieldParallel(const std::vector<sol::coroutine> &coroutines) {
    if (coroutines.empty()) {
        m_Engine->GetLogger()->Error("YieldParallel: no coroutines provided");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldParallel called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<ParallelTask>(coroutines));
}

void LuaScheduler::YieldRace(const std::vector<sol::coroutine> &coroutines) {
    if (coroutines.empty()) {
        m_Engine->GetLogger()->Error("YieldRace: no coroutines provided");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldRace called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<RaceTask>(coroutines));
}

void LuaScheduler::YieldRetry(const sol::function &task, int max_attempts) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("YieldRetry: invalid task function");
        return;
    }

    if (max_attempts <= 0) {
        m_Engine->GetLogger()->Error("YieldRetry: max attempts must be positive");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldRetry called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<RetryTask>(task, max_attempts));
}

void LuaScheduler::YieldWaitForEvent(const std::string &event_name) {
    if (event_name.empty()) {
        m_Engine->GetLogger()->Error("YieldWaitForEvent: event name cannot be empty");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldWaitForEvent called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<EventWaitTask>(event_name, m_Engine));
}

void LuaScheduler::YieldDebounce(const sol::function &task, int debounce_ticks) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("YieldDebounce: invalid task function");
        return;
    }

    if (debounce_ticks <= 0) {
        m_Engine->GetLogger()->Error("YieldDebounce: debounce ticks must be positive");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldDebounce called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<DebounceTask>(task, debounce_ticks));
}

void LuaScheduler::Yield(std::shared_ptr<SchedulerTask> task) {
    detail::SchedulerThreadTask thread_task;
    thread_task.thread = m_CurrentThread;
    thread_task.task = std::move(task);
    m_Tasks.push_back(thread_task);
}
