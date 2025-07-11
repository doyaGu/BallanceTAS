#include "LuaScheduler.h"

#include <utility>

#include "TASEngine.h"

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

void LuaScheduler::Yield(std::shared_ptr<SchedulerTask> task) {
    detail::SchedulerThreadTask thread_task;
    thread_task.thread = m_CurrentThread;
    thread_task.task = std::move(task);
    m_Tasks.push_back(thread_task);
}
