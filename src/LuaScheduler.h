#pragma once

#include <vector>
#include <list>
#include <stack>
#include <memory>
#include <functional>

#include <sol/sol.hpp>

// Forward declare to avoid circular dependency
class TASEngine;

/**
 * @class SchedulerTask
 * @brief Base class for scheduler tasks that define completion conditions.
 */
class SchedulerTask {
public:
    virtual ~SchedulerTask() = default;
    virtual bool IsComplete() = 0;
};

class ImmediateTask : public SchedulerTask {
public:
    ImmediateTask() = default;

    bool IsComplete() override {
        return true; // Immediate tasks are always complete
    }
};

/**
 * @class TickWaitTask
 * @brief Task that completes after a specified number of ticks.
 */
class TickWaitTask : public SchedulerTask {
public:
    explicit TickWaitTask(int ticks) : m_RemainingTicks(ticks) {}

    bool IsComplete() override {
        --m_RemainingTicks;
        return m_RemainingTicks <= 0;
    }

private:
    int m_RemainingTicks;
};

/**
 * @class PredicateWaitTask
 * @brief Task that completes when a predicate function returns true.
 */
class PredicateWaitTask : public SchedulerTask {
public:
    explicit PredicateWaitTask(sol::function predicate) : m_Predicate(std::move(predicate)) {}

    bool IsComplete() override {
        if (!m_Predicate.valid()) return true;
        try {
            return m_Predicate();
        } catch (const std::exception &) {
            return true; // Complete on error to avoid hanging
        }
    }

private:
    sol::function m_Predicate;
};

/**
 * @class CoroutineWaitTask
 * @brief Task that completes when all specified coroutines are done.
 */
class CoroutineWaitTask : public SchedulerTask {
public:
    explicit CoroutineWaitTask(std::vector<sol::coroutine> coroutines)
        : m_Coroutines(std::move(coroutines)) {}

    bool IsComplete() override {
        for (const auto &co : m_Coroutines) {
            if (co.valid() && co.status() == sol::call_status::yielded) {
                return false; // At least one still running
            }
        }
        return true; // All done or invalid
    }

private:
    std::vector<sol::coroutine> m_Coroutines;
};

/**
 * @class RepeatTask
 * @brief Base class for repeating tasks
 */
class RepeatTask : public SchedulerTask {
public:
    RepeatTask(sol::function task) : m_Task(std::move(task)), m_ShouldStop(false) {}

protected:
    sol::function m_Task;
    bool m_ShouldStop;

    void ExecuteTask() {
        if (m_Task.valid()) {
            try {
                m_Task();
            } catch (const std::exception &) {
                // Log error but don't stop execution unless critical
            }
        }
    }
};

/**
 * @class RepeatForTicksTask
 * @brief Repeats a task for a specified number of ticks
 */
class RepeatForTicksTask : public RepeatTask {
public:
    RepeatForTicksTask(sol::function task, int ticks)
        : RepeatTask(std::move(task)), m_RemainingTicks(ticks) {}

    bool IsComplete() override {
        if (m_ShouldStop || m_RemainingTicks <= 0) {
            return true;
        }

        ExecuteTask();
        --m_RemainingTicks;
        return m_RemainingTicks <= 0;
    }

private:
    int m_RemainingTicks;
};

/**
 * @class RepeatUntilTask
 * @brief Repeats a task until a condition becomes true
 */
class RepeatUntilTask : public RepeatTask {
public:
    RepeatUntilTask(sol::function task, sol::function condition)
        : RepeatTask(std::move(task)), m_Condition(std::move(condition)) {}

    bool IsComplete() override {
        if (m_ShouldStop) return true;

        // Check condition first
        bool conditionMet = false;
        if (m_Condition.valid()) {
            try {
                conditionMet = m_Condition();
            } catch (const std::exception &) {
                return true; // Complete on error
            }
        }

        if (conditionMet) return true;

        // Execute task and continue
        ExecuteTask();
        return false;
    }

private:
    sol::function m_Condition;
};

/**
 * @class RepeatWhileTask
 * @brief Repeats a task while a condition is true
 */
class RepeatWhileTask : public RepeatTask {
public:
    RepeatWhileTask(sol::function task, sol::function condition)
        : RepeatTask(std::move(task)), m_Condition(std::move(condition)) {}

    bool IsComplete() override {
        if (m_ShouldStop) return true;

        // Check condition first
        bool conditionMet = true;
        if (m_Condition.valid()) {
            try {
                conditionMet = m_Condition();
            } catch (const std::exception &) {
                return true; // Complete on error
            }
        }

        if (!conditionMet) return true;

        // Execute task and continue
        ExecuteTask();
        return false;
    }

private:
    sol::function m_Condition;
};

/**
 * @class RepeatCountTask
 * @brief Repeats a task a specific number of times
 */
class RepeatCountTask : public RepeatTask {
public:
    RepeatCountTask(sol::function task, int count)
        : RepeatTask(std::move(task)), m_RemainingCount(count) {}

    bool IsComplete() override {
        if (m_ShouldStop || m_RemainingCount <= 0) {
            return true;
        }

        ExecuteTask();
        --m_RemainingCount;
        return m_RemainingCount <= 0;
    }

private:
    int m_RemainingCount;
};

/**
 * @class DelayTask
 * @brief Delays execution of a task by specified ticks
 */
class DelayTask : public SchedulerTask {
public:
    DelayTask(sol::function task, int delayTicks)
        : m_Task(std::move(task)), m_DelayTicks(delayTicks), m_TaskExecuted(false) {}

    bool IsComplete() override {
        if (m_DelayTicks > 0) {
            --m_DelayTicks;
            return false;
        }

        if (!m_TaskExecuted) {
            if (m_Task.valid()) {
                try {
                    m_Task();
                } catch (const std::exception &) {
                    // Log error
                }
            }
            m_TaskExecuted = true;
        }

        return true;
    }

private:
    sol::function m_Task;
    int m_DelayTicks;
    bool m_TaskExecuted;
};

/**
 * @class TimeoutTask
 * @brief Runs a task with a timeout
 */
class TimeoutTask : public SchedulerTask {
public:
    TimeoutTask(sol::function task, int timeoutTicks)
        : m_Task(std::move(task)), m_TimeoutTicks(timeoutTicks), m_TaskComplete(false) {}

    bool IsComplete() override {
        if (m_TaskComplete) return true;

        if (m_TimeoutTicks <= 0) {
            return true; // Timeout reached
        }

        --m_TimeoutTicks;

        // Execute task (assume it's a check or single operation)
        if (m_Task.valid()) {
            try {
                bool result = m_Task();
                if (result) {
                    m_TaskComplete = true;
                    return true;
                }
            } catch (const std::exception &) {
                return true; // Complete on error
            }
        }

        return false;
    }

private:
    sol::function m_Task;
    int m_TimeoutTicks;
    bool m_TaskComplete;
};

/**
 * @class SequenceTask
 * @brief Executes multiple tasks in sequence
 */
class SequenceTask : public SchedulerTask {
public:
    explicit SequenceTask(std::vector<sol::function> tasks)
        : m_Tasks(std::move(tasks)), m_CurrentIndex(0) {}

    bool IsComplete() override {
        if (m_CurrentIndex >= m_Tasks.size()) {
            return true;
        }

        // Execute current task
        if (m_Tasks[m_CurrentIndex].valid()) {
            try {
                m_Tasks[m_CurrentIndex]();
            } catch (const std::exception &) {
                // Log error and continue to next task
            }
        }

        ++m_CurrentIndex;
        return m_CurrentIndex >= m_Tasks.size();
    }

private:
    std::vector<sol::function> m_Tasks;
    size_t m_CurrentIndex;
};

/**
 * @class ParallelTask
 * @brief Runs multiple tasks in parallel and waits for all to complete
 */
class ParallelTask : public SchedulerTask {
public:
    explicit ParallelTask(std::vector<sol::coroutine> coroutines)
        : m_Coroutines(std::move(coroutines)) {}

    bool IsComplete() override {
        for (const auto &co : m_Coroutines) {
            if (co.valid() && co.status() == sol::call_status::yielded) {
                return false; // At least one still running
            }
        }
        return true; // All done or invalid
    }

private:
    std::vector<sol::coroutine> m_Coroutines;
};

/**
 * @class RaceTask
 * @brief Runs multiple tasks in parallel and completes when first one finishes
 */
class RaceTask : public SchedulerTask {
public:
    explicit RaceTask(std::vector<sol::coroutine> coroutines)
        : m_Coroutines(std::move(coroutines)), m_SomeCompleted(false) {}

    bool IsComplete() override {
        if (m_SomeCompleted) return true;

        for (const auto &co : m_Coroutines) {
            if (!co.valid() || co.status() != sol::call_status::yielded) {
                m_SomeCompleted = true;
                return true; // At least one completed
            }
        }
        return false; // All still running
    }

private:
    std::vector<sol::coroutine> m_Coroutines;
    bool m_SomeCompleted;
};

/**
 * @class RetryTask
 * @brief Retries a task up to a maximum number of attempts
 */
class RetryTask : public SchedulerTask {
public:
    RetryTask(sol::function task, int maxAttempts)
        : m_Task(std::move(task)), m_MaxAttempts(maxAttempts), m_CurrentAttempt(0) {}

    bool IsComplete() override {
        if (m_CurrentAttempt >= m_MaxAttempts) {
            return true;
        }

        ++m_CurrentAttempt;

        if (m_Task.valid()) {
            try {
                if (m_Task()) {
                    return true; // Task succeeded
                }
            } catch (const std::exception &) {
                // Continue to next attempt
            }
        }

        return m_CurrentAttempt >= m_MaxAttempts;
    }

private:
    sol::function m_Task;
    int m_MaxAttempts;
    int m_CurrentAttempt;
};

/**
 * @class EventWaitTask
 * @brief Waits for a specific event to be triggered
 */
class EventWaitTask : public SchedulerTask {
public:
    EventWaitTask(std::string eventName, TASEngine *engine);

    bool IsComplete() override {
        return m_EventReceived;
    }

private:
    std::string m_EventName;
    TASEngine *m_Engine;
    bool m_EventReceived;
};

/**
 * @class DebounceTask
 * @brief Debounces task execution - only executes after delay with no new calls
 */
class DebounceTask : public SchedulerTask {
public:
    DebounceTask(sol::function task, int debounceTicks)
        : m_Task(std::move(task)),
          m_DebounceTicks(debounceTicks),
          m_RemainingTicks(debounceTicks),
          m_TaskExecuted(false) {}

    bool IsComplete() override {
        if (m_TaskExecuted) return true;

        --m_RemainingTicks;

        if (m_RemainingTicks <= 0) {
            if (m_Task.valid()) {
                try {
                    m_Task();
                } catch (const std::exception &) {
                    // Log error
                }
            }
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
    sol::function m_Task;
    int m_DebounceTicks;
    int m_RemainingTicks;
    bool m_TaskExecuted;
};

namespace detail {
    struct SchedulerCothread {
        sol::thread thread;
        sol::coroutine coroutine;

        SchedulerCothread(sol::state &state, sol::function func) {
            thread = sol::thread::create(state);
            coroutine = sol::coroutine(thread.state(), func);
        }
    };

    struct SchedulerThreadTask {
        std::shared_ptr<SchedulerCothread> thread;
        std::shared_ptr<SchedulerTask> task;
    };
}

/**
 * @class LuaScheduler
 * @brief Manages the execution of Lua coroutines using sol2's proper threading model.
 */
class LuaScheduler {
public:
    explicit LuaScheduler(TASEngine *engine);
    ~LuaScheduler() = default;

    sol::state &GetLuaState() const;

    /**
     * @brief Starts a new coroutine.
     * @param co Coroutine to run.
     */
    void StartCoroutine(sol::coroutine co);

    /**
     * @brief Starts a new coroutine from a function.
     * @param func Lua function to run as coroutine.
     */
    void StartCoroutine(sol::function func);

    /**
     * @brief Adds a coroutine task to the scheduler.
     * @param co Coroutine to add.
     */
    void AddCoroutineTask(sol::coroutine co);

    /**
     * @brief Adds a coroutine task from a Lua function.
     * @param func Lua function to add as a coroutine task.
     */
    void AddCoroutineTask(sol::function func);

    /**
     * @brief The main update loop - processes all scheduled tasks.
     */
    void Tick();

    /**
     * @brief Stops all running coroutines and clears tasks.
     */
    void Clear();

    /**
     * @brief Checks if there are any active scripts running.
     */
    bool IsRunning() const;

    /**
     * @brief Gets the number of pending tasks.
     */
    size_t GetTaskCount() const;

    // --- Yielding methods for sol::yielding functions ---

    /**
     * @brief Yields current coroutine for specified ticks.
     * This is called from sol::yielding functions.
     */
    void YieldTicks(int ticks);

    /**
     * @brief Yields current coroutine until predicate is true.
     */
    void YieldUntil(sol::function predicate);

    /**
     * @brief Yields current coroutine until other coroutines complete.
     */
    void YieldCoroutines(const std::vector<sol::coroutine> &coroutines);

    /**
     * @brief Repeats a task for specified number of ticks
     */
    void YieldRepeatFor(sol::function task, int ticks);

    /**
     * @brief Repeats a task until condition becomes true
     */
    void YieldRepeatUntil(sol::function task, sol::function condition);

    /**
     * @brief Repeats a task while condition is true
     */
    void YieldRepeatWhile(sol::function task, sol::function condition);

    /**
     * @brief Repeats a task a specific number of times
     */
    void YieldRepeatCount(sol::function task, int count);

    /**
     * @brief Delays execution of a task by specified ticks
     */
    void YieldDelay(sol::function task, int delayTicks);

    /**
     * @brief Runs a task with timeout
     */
    void YieldTimeout(sol::function task, int timeoutTicks);

    /**
     * @brief Executes tasks in sequence
     */
    void YieldSequence(const std::vector<sol::function> &tasks);

    /**
     * @brief Runs tasks in parallel and waits for all to complete
     */
    void YieldParallel(const std::vector<sol::coroutine> &coroutines);

    /**
     * @brief Runs tasks in parallel and completes when first one finishes
     */
    void YieldRace(const std::vector<sol::coroutine> &coroutines);

    /**
     * @brief Retries a task up to max_attempts times
     */
    void YieldRetry(sol::function task, int maxAttempts);

    /**
     * @brief Waits for a specific event
     */
    void YieldWaitForEvent(const std::string &event_name);

    /**
     * @brief Debounces task execution
     */
    void YieldDebounce(sol::function task, int debounceTicks);

private:
    void Yield(std::shared_ptr<SchedulerTask> task);

    TASEngine *m_Engine;
    std::shared_ptr<detail::SchedulerCothread> m_CurrentThread;
    std::list<detail::SchedulerThreadTask> m_Tasks;
    std::stack<std::shared_ptr<detail::SchedulerCothread>> m_ThreadStack;
};
