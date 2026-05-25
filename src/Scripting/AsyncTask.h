#pragma once

#include <string>
#include <chrono>
#include <memory>

#include "LuaRuntime/LuaThread.h"
#include "LuaRuntime/LuaValue.h"

// Forward declarations
class LuaScheduler;
class ScriptContext;

/**
 * @brief Async task state
 */
enum class AsyncTaskState {
    Pending,   // Not started or waiting
    Running,   // Currently executing
    Completed, // Finished successfully
    Failed,    // Finished with error
    Cancelled  // Cancelled by user
};

/**
 * @brief Promise-like async task wrapper for Lua coroutines
 *
 * Provides a modern async/await API on top of LuaScheduler's coroutine system.
 * Tasks can be awaited, chained with then/catch, and combined with all/race/any.
 *
 * Example usage:
 * @code
 *   local task = tas.async(function()
 *       tas.await(tas.delay(10))
 *       return tas.input.get_key("space")
 *   end)
 *
 *   local result = task:await()
 * @endcode
 */
class AsyncTask {
public:
    /**
     * @brief Creates a new async task
     * @param scheduler The Lua scheduler to use
     * @param coroutine The Lua coroutine to wrap
     * @param context The script context
     */
    AsyncTask(LuaScheduler *scheduler, tas::lua::LuaThread coroutine, ScriptContext *context);

    ~AsyncTask() = default;

    // AsyncTask is not copyable but is movable
    AsyncTask(const AsyncTask &) = delete;
    AsyncTask &operator=(const AsyncTask &) = delete;
    AsyncTask(AsyncTask &&) = default;
    AsyncTask &operator=(AsyncTask &&) = default;

    /**
     * @brief Gets the current task state
     */
    AsyncTaskState GetState() const { return m_State; }

    /**
     * @brief Checks if task is pending
     */
    bool IsPending() const { return m_State == AsyncTaskState::Pending; }

    /**
     * @brief Checks if task is running
     */
    bool IsRunning() const { return m_State == AsyncTaskState::Running; }

    /**
     * @brief Checks if task is completed
     */
    bool IsCompleted() const { return m_State == AsyncTaskState::Completed; }

    /**
     * @brief Checks if task is failed
     */
    bool IsFailed() const { return m_State == AsyncTaskState::Failed; }

    /**
     * @brief Checks if task is cancelled
     */
    bool IsCancelled() const { return m_State == AsyncTaskState::Cancelled; }

    /**
     * @brief Checks if task is done (completed, failed, or cancelled)
     */
    bool IsDone() const {
        return m_State == AsyncTaskState::Completed ||
               m_State == AsyncTaskState::Failed ||
               m_State == AsyncTaskState::Cancelled;
    }

    /**
     * @brief Gets the task result (if completed)
     * @return Result object, or nil if not completed
     */
    const tas::lua::LuaValue &GetResult() const { return m_Result; }

    /**
     * @brief Gets the task error (if failed)
     * @return Error string, or empty if not failed
     */
    std::string GetError() const { return m_Error; }

    /**
     * @brief Starts the task execution
     */
    void Start();

    /**
     * @brief Cancels the task
     */
    void Cancel();

    /**
     * @brief Polls the task (advances execution)
     * @return True if task needs more polling, false if done
     */
    bool Poll();

    /**
     * @brief Sets the result (called when task completes)
     */
    void SetResult(tas::lua::LuaValue result);

    /**
     * @brief Sets the error (called when task fails)
     */
    void SetError(const std::string &error);

    /**
     * @brief Gets the underlying coroutine
     */
    tas::lua::LuaThread &GetCoroutine() { return *m_Coroutine; }
    const tas::lua::LuaThread &GetCoroutine() const { return *m_Coroutine; }
    std::shared_ptr<tas::lua::LuaThread> GetThread() const { return m_Coroutine; }
    LuaScheduler *GetScheduler() const { return m_Scheduler; }

    /**
     * @brief Gets the coroutine ID (from scheduler)
     */
    int GetCoroutineId() const { return m_CoroutineId; }
    bool IsScheduled() const { return m_Scheduled; }
    void MarkScheduled() { m_Scheduled = true; }

private:
    LuaScheduler *m_Scheduler;
    ScriptContext *m_Context;
    std::shared_ptr<tas::lua::LuaThread> m_Coroutine;
    int m_CoroutineId = -1;

    AsyncTaskState m_State = AsyncTaskState::Pending;
    tas::lua::LuaValue m_Result;
    std::string m_Error;
    bool m_Scheduled = false;
};
