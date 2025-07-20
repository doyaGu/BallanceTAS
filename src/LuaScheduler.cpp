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

void LuaScheduler::StartCoroutine(sol::coroutine co) {
    // Check if coroutine is valid
    if (!co.valid()) {
        m_Engine->GetLogger()->Error("StartCoroutine: invalid coroutine provided");
        return;
    }

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

void LuaScheduler::StartCoroutine(sol::function func) {
    if (!func.valid()) {
        m_Engine->GetLogger()->Error("StartCoroutine: invalid function provided");
        return;
    }

    // Create a new thread context for the coroutine
    auto thread = std::make_shared<detail::SchedulerCothread>(GetLuaState(), func);

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

sol::coroutine LuaScheduler::StartCoroutineAndTrack(sol::function func) {
    if (!func.valid()) {
        m_Engine->GetLogger()->Error("StartCoroutineAndTrack: invalid function provided");
        return {};
    }

    // Create new thread for the coroutine
    auto thread = std::make_shared<detail::SchedulerCothread>(GetLuaState(), func);

    // Create an immediate task that will cause the coroutine to start on next tick
    auto task = std::make_shared<ImmediateTask>();

    // Add to task list
    detail::SchedulerThreadTask threadTask;
    threadTask.thread = thread;
    threadTask.task = task;
    m_Tasks.push_back(threadTask);

    // Return the coroutine reference for tracking
    return thread->coroutine;
}

sol::coroutine LuaScheduler::StartCoroutineAndTrack(sol::coroutine co) {
    if (!co.valid()) {
        m_Engine->GetLogger()->Error("StartCoroutineAndTrack: invalid coroutine provided");
        return {};
    }

    // Create thread for existing coroutine (handles thread management internally)
    auto thread = std::make_shared<detail::SchedulerCothread>(GetLuaState(), co);

    // Create an immediate task
    auto task = std::make_shared<ImmediateTask>();

    // Add to task list
    detail::SchedulerThreadTask threadTask;
    threadTask.thread = thread;
    threadTask.task = task;
    m_Tasks.push_back(threadTask);

    // Return the coroutine reference for tracking
    return thread->coroutine;
}

void LuaScheduler::AddCoroutineTask(sol::coroutine co) {
    if (!co.valid()) {
        m_Engine->GetLogger()->Error("AddCoroutineTask: invalid coroutine provided");
        return;
    }

    // Create thread for existing coroutine
    auto thread = std::make_shared<detail::SchedulerCothread>(GetLuaState(), co);

    // Create an immediate task
    auto task = std::make_shared<ImmediateTask>();

    // Add to task list
    detail::SchedulerThreadTask threadTask;
    threadTask.thread = thread;
    threadTask.task = task;
    m_Tasks.push_back(threadTask);
}

void LuaScheduler::AddCoroutineTask(sol::function func) {
    if (!func.valid()) {
        m_Engine->GetLogger()->Error("AddCoroutineTask: invalid function provided");
        return;
    }

    // For functions, create new thread directly
    auto thread = std::make_shared<detail::SchedulerCothread>(GetLuaState(), func);

    // Create an immediate task
    auto task = std::make_shared<ImmediateTask>();

    // Add to task list
    detail::SchedulerThreadTask threadTask;
    threadTask.thread = thread;
    threadTask.task = task;
    m_Tasks.push_back(threadTask);
}

void LuaScheduler::Tick() {
    // Process coroutine-based tasks
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

    // Process background tasks
    for (auto i = m_BackgroundTasks.begin(); i != m_BackgroundTasks.end();) {
        if ((*i)->IsComplete()) {
            i = m_BackgroundTasks.erase(i);
        } else {
            ++i;
        }
    }
}

void LuaScheduler::Clear() {
    m_Tasks.clear();
    m_BackgroundTasks.clear();
    m_CurrentThread = nullptr;
    // Clear the thread stack
    while (!m_ThreadStack.empty()) {
        m_ThreadStack.pop();
    }
}

bool LuaScheduler::IsRunning() const {
    return !m_Tasks.empty() || !m_BackgroundTasks.empty();
}

size_t LuaScheduler::GetTaskCount() const {
    return m_Tasks.size() + m_BackgroundTasks.size();
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

void LuaScheduler::YieldUntil(sol::function predicate) {
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

void LuaScheduler::YieldRace(const std::vector<sol::coroutine> &coroutines) {
    if (coroutines.empty()) {
        m_Engine->GetLogger()->Error("YieldRace: no coroutines to wait for");
        return;
    }

    if (!m_CurrentThread) {
        m_Engine->GetLogger()->Error("YieldRace called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<RaceTask>(coroutines));
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

void LuaScheduler::StartRepeatFor(sol::function task, int ticks) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("StartRepeatFor: invalid task function");
        return;
    }

    if (ticks <= 0) {
        m_Engine->GetLogger()->Error("StartRepeatFor: tick count must be positive");
        return;
    }

    auto repeatTask = std::make_shared<RepeatForTicksTask>(task, ticks);
    m_BackgroundTasks.push_back(repeatTask);
}

void LuaScheduler::StartRepeatUntil(sol::function task, sol::function condition) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("StartRepeatUntil: invalid task function");
        return;
    }

    if (!condition.valid()) {
        m_Engine->GetLogger()->Error("StartRepeatUntil: invalid condition function");
        return;
    }

    auto repeatTask = std::make_shared<RepeatUntilTask>(task, condition);
    m_BackgroundTasks.push_back(repeatTask);
}

void LuaScheduler::StartRepeatWhile(sol::function task, sol::function condition) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("StartRepeatWhile: invalid task function");
        return;
    }

    if (!condition.valid()) {
        m_Engine->GetLogger()->Error("StartRepeatWhile: invalid condition function");
        return;
    }

    auto repeatTask = std::make_shared<RepeatWhileTask>(task, condition);
    m_BackgroundTasks.push_back(repeatTask);
}

void LuaScheduler::StartDelay(sol::function task, int delayTicks) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("StartDelay: invalid task function");
        return;
    }

    if (delayTicks < 0) {
        m_Engine->GetLogger()->Error("StartDelay: delay ticks cannot be negative");
        return;
    }

    auto delayTask = std::make_shared<DelayTask>(task, delayTicks);
    m_BackgroundTasks.push_back(delayTask);
}

void LuaScheduler::StartTimeout(sol::function task, int timeoutTicks) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("StartTimeout: invalid task function");
        return;
    }

    if (timeoutTicks <= 0) {
        m_Engine->GetLogger()->Error("StartTimeout: timeout must be positive");
        return;
    }

    auto timeoutTask = std::make_shared<TimeoutTask>(task, timeoutTicks);
    m_BackgroundTasks.push_back(timeoutTask);
}

void LuaScheduler::StartDebounce(sol::function task, int debounceTicks) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("StartDebounce: invalid task function");
        return;
    }

    if (debounceTicks <= 0) {
        m_Engine->GetLogger()->Error("StartDebounce: debounce ticks must be positive");
        return;
    }

    auto debounceTask = std::make_shared<DebounceTask>(task, debounceTicks);
    m_BackgroundTasks.push_back(debounceTask);
}

void LuaScheduler::StartSequence(const std::vector<sol::function> &tasks) {
    if (tasks.empty()) {
        m_Engine->GetLogger()->Error("StartSequence: no tasks provided");
        return;
    }

    auto sequenceTask = std::make_shared<SequenceTask>(tasks);
    m_BackgroundTasks.push_back(sequenceTask);
}

void LuaScheduler::StartRetry(sol::function task, int maxAttempts) {
    if (!task.valid()) {
        m_Engine->GetLogger()->Error("StartRetry: invalid task function");
        return;
    }

    if (maxAttempts <= 0) {
        m_Engine->GetLogger()->Error("StartRetry: max attempts must be positive");
        return;
    }

    auto retryTask = std::make_shared<RetryTask>(task, maxAttempts);
    m_BackgroundTasks.push_back(retryTask);
}

void LuaScheduler::StartParallel(const std::vector<sol::function> &functions) {
    for (const auto &func : functions) {
        if (func.valid()) {
            AddCoroutineTask(func);
        }
    }
}

void LuaScheduler::StartParallel(const std::vector<sol::coroutine> &coroutines) {
    for (const auto &co : coroutines) {
        if (co.valid()) {
            AddCoroutineTask(co);
        }
    }
}

void LuaScheduler::Yield(std::shared_ptr<SchedulerTask> task) {
    detail::SchedulerThreadTask thread_task;
    thread_task.thread = m_CurrentThread;
    thread_task.task = std::move(task);
    m_Tasks.push_back(thread_task);
}
