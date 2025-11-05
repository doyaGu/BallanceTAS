#pragma once

#include <vector>
#include <list>
#include <stack>
#include <memory>
#include <cstdint>
#include <any>

#include <sol/sol.hpp>

#include "ThreadOwnershipValidator.h"

// Forward declare to avoid circular dependency
class TASEngine;
class ScriptContext;

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
 * @class RaceTask
 * @brief Task that completes when first coroutine finishes
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
    EventWaitTask(std::string eventName, TASEngine *engine, class EventManager *eventManager = nullptr);
    ~EventWaitTask() override;

    bool IsComplete() override {
        return m_EventReceived;
    }

private:
    std::string m_EventName;
    TASEngine *m_Engine;
    class EventManager *m_EventManager; // Context's event manager (or engine's if legacy)
    bool m_EventReceived;
    uint64_t m_ListenerId = 0;
};

// Forward declare MessageBus::Message::SerializedValue
class MessageBus;
namespace MessageBusInternal {
    struct SerializedValue;
}

/**
 * @class MessageResponseTask
 * @brief Waits for a message response with a specific correlation ID
 */
class MessageResponseTask : public SchedulerTask {
public:
    MessageResponseTask(std::string correlationId, TASEngine *engine, int timeoutTicks);
    ~MessageResponseTask() override;

    bool IsComplete() override;

    /**
     * @brief Gets the response data if available.
     * @return Response object, or nil if timeout/error.
     */
    sol::object GetResponse(sol::state_view lua) const;

    /**
     * @brief Checks if response was received (vs timeout).
     * @return True if response received, false if timeout.
     */
    bool HasResponse() const { return m_ResponseReceived; }

private:
    std::string m_CorrelationId;
    TASEngine *m_Engine;
    int m_TimeoutTicks;
    bool m_ResponseReceived;
    std::any m_ResponseData;  // Stores MessageBus::Message::SerializedValue as std::any to avoid forward declaration issues
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

        // Constructor for creating coroutine from function
        SchedulerCothread(sol::state &state, sol::function func) {
            if (!func.valid()) {
                throw std::invalid_argument("Invalid function provided to SchedulerCothread");
            }
            thread = sol::thread::create(state);
            coroutine = sol::coroutine(thread.thread_state(), func);
        }

        // Constructor for existing coroutines
        // IMPORTANT: This constructor is safe when the coroutine is already
        // on its own thread. If the coroutine is on the main state, it will
        // create a new thread but reuse the coroutine reference.
        //
        // THREAD SAFETY NOTE:
        // - If co is already on a thread: Safe, wraps the existing thread
        // - If co is on main state: Creates new thread, reuses coroutine
        //   This is safe in Lua 5.1+ as coroutines can be transferred between threads
        //   as long as they're not currently running.
        SchedulerCothread(sol::state &state, sol::coroutine co) {
            if (!co.valid()) {
                throw std::invalid_argument("Invalid coroutine provided to SchedulerCothread");
            }

            // Get the coroutine's current thread state
            lua_State* co_state = co.lua_state();

            if (co_state && co_state != state.lua_state()) {
                // Coroutine is on a different thread, wrap that thread
                thread = sol::thread(state.lua_state(), co_state);
                coroutine = co; // Use the existing coroutine as-is
            } else {
                // Coroutine is on main state, create new thread
                thread = sol::thread::create(state);
                // Reuse the coroutine - this is safe as long as the coroutine
                // is not currently executing. The scheduler ensures this by only
                // calling this constructor for yielded or fresh coroutines.
                coroutine = co;
            }
        }

        // Factory method for safer handling of existing coroutines
        static std::shared_ptr<SchedulerCothread> CreateFromExistingCoroutine(
            sol::state &state, sol::coroutine co) {

            if (!co.valid()) {
                throw std::invalid_argument("Invalid coroutine provided");
            }

            return std::make_shared<SchedulerCothread>(state, co);
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
 *
 * THREAD SAFETY:
 * ==============
 * LuaScheduler is NOT thread-safe and MUST be called from a single thread only.
 * Each ScriptContext owns its own LuaScheduler instance, and all methods of the
 * scheduler (including Tick(), StartCoroutine(), YieldXxx(), etc.) must be called
 * from the same thread that processes that ScriptContext.
 *
 * CRITICAL: The Tick() method modifies internal state (m_CurrentThread, m_ThreadStack,
 * m_Tasks) without synchronization. Calling Tick() from multiple threads will result
 * in undefined behavior, data races, and crashes.
 *
 * In typical usage:
 * - The main game thread calls TASEngine::Tick()
 * - TASEngine::Tick() calls ScriptContext::Tick() for each context
 * - ScriptContext::Tick() calls LuaScheduler::Tick()
 * All of this happens on a single thread per context.
 *
 * If multi-threaded context execution is required in the future, each ScriptContext
 * must have its own dedicated thread, OR all LuaScheduler methods must be protected
 * with a per-scheduler mutex.
 */
class LuaScheduler {
public:
    /**
     * @param engine The TASEngine instance (required)
     * @param context The ScriptContext instance (optional, for multi-context mode)
     */
    explicit LuaScheduler(TASEngine *engine, ScriptContext *context);
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
     * @brief Starts a coroutine and returns a reference for tracking.
     * @param func Lua function to run as coroutine.
     * @return Coroutine reference for tracking.
     */
    sol::coroutine StartCoroutineAndTrack(sol::function func);

    /**
     * @brief Starts a coroutine and returns a reference for tracking.
     * @param co Coroutine to run.
     * @return Coroutine reference for tracking.
     */
    sol::coroutine StartCoroutineAndTrack(sol::coroutine co);

    /**
     * @brief The main update loop - processes all scheduled tasks.
     *
     * THREAD SAFETY WARNING:
     * This method is NOT thread-safe and MUST be called from a single thread only.
     * Calling Tick() from multiple threads will cause data races on m_CurrentThread,
     * m_ThreadStack, and m_Tasks, resulting in undefined behavior and crashes.
     *
     * Thread ownership is now enforced via ThreadOwnershipValidator in debug builds.
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
     * @brief Yields current coroutine until first coroutine completes.
     */
    void YieldRace(const std::vector<sol::coroutine> &coroutines);

    /**
     * @brief Waits for a specific event
     */
    void YieldWaitForEvent(const std::string &event_name);

    /**
     * @brief Waits for a message response with timeout
     * @param correlationId The correlation ID to wait for
     * @param timeoutMs Timeout in milliseconds (converted to ticks internally)
     * @return Response data if received, or nil if timeout
     */
    sol::object YieldWaitForMessageResponse(const std::string &correlationId, int timeoutMs);

    // --- Background task methods (non-yielding) ---

    /**
     * @brief Starts background operations that don't yield the current script
     */

    // Background repeat operations
    void StartRepeatFor(sol::function task, int ticks);
    void StartRepeatUntil(sol::function task, sol::function condition);
    void StartRepeatWhile(sol::function task, sol::function condition);

    // Background timing operations
    void StartDelay(sol::function task, int delayTicks);
    void StartTimeout(sol::function task, int timeoutTicks);
    void StartDebounce(sol::function task, int debounceTicks);

    // Background control flow operations
    void StartSequence(const std::vector<sol::function> &tasks);
    void StartRetry(sol::function task, int maxAttempts);

    // Background parallel operations
    void StartParallel(const std::vector<sol::function> &functions);
    void StartParallel(const std::vector<sol::coroutine> &coroutines);

private:
    void Yield(std::shared_ptr<SchedulerTask> task);

    TASEngine *m_Engine;
    ScriptContext *m_Context; // nullptr for legacy mode, non-null for multi-context mode
    std::shared_ptr<detail::SchedulerCothread> m_CurrentThread;
    std::list<detail::SchedulerThreadTask> m_Tasks;
    std::list<std::shared_ptr<SchedulerTask>> m_BackgroundTasks;
    std::stack<std::shared_ptr<detail::SchedulerCothread>> m_ThreadStack;

    // Thread safety enforcement
    mutable ThreadOwnershipValidator m_ThreadValidator{"LuaScheduler"};
};
