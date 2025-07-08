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

namespace detail {
    struct SchedulerCothread {
        sol::thread thread;
        sol::coroutine coroutine;

        SchedulerCothread(sol::state &state, const sol::function &func) {
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
    void StartCoroutine(const sol::coroutine &co);

    /**
     * @brief Starts a new coroutine from a function.
     * @param func Lua function to run as coroutine.
     */
    void StartCoroutine(const sol::function &func);

    /**
     * @brief Adds a coroutine task to the scheduler.
     * @param co Coroutine to add.
     */
    void AddCoroutineTask(const sol::coroutine &co);

    /**
     * @brief Adds a coroutine task from a Lua function.
     * @param func Lua function to add as a coroutine task.
     */
    void AddCoroutineTask(const sol::function &func);

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
    void YieldUntil(const sol::function &predicate);

    /**
     * @brief Yields current coroutine until other coroutines complete.
     */
    void YieldCoroutines(const std::vector<sol::coroutine> &coroutines);

private:
    void Yield(std::shared_ptr<SchedulerTask> task);

    TASEngine *m_Engine;
    std::shared_ptr<detail::SchedulerCothread> m_CurrentThread;
    std::list<detail::SchedulerThreadTask> m_Tasks;
    std::stack<std::shared_ptr<detail::SchedulerCothread>> m_ThreadStack;
};
