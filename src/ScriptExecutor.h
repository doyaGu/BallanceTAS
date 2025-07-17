#pragma once

#include <sol/sol.hpp>
#include <string>
#include <memory>

// Forward declarations
class TASEngine;
class TASProject;
class LuaScheduler;

/**
 * @class ScriptExecutor
 * @brief Handles execution and lifecycle management of Lua TAS scripts.
 *
 * This class is responsible for loading, executing, and managing Lua-based
 * TAS projects. It provides a clean interface for script execution while
 * handling all the Lua-specific details internally.
 */
class ScriptExecutor {
public:
    explicit ScriptExecutor(TASEngine *engine);
    ~ScriptExecutor();

    // ScriptExecutor is not copyable or movable
    ScriptExecutor(const ScriptExecutor &) = delete;
    ScriptExecutor &operator=(const ScriptExecutor &) = delete;

    /**
     * @brief Initializes the script execution environment.
     * @return True if initialization was successful.
     */
    bool Initialize();

    /**
     * @brief Shuts down the script execution environment.
     */
    void Shutdown();

    /**
     * @brief Loads and starts executing a TAS script project.
     * @param project The script-based TAS project to execute.
     * @return True if the script was loaded and started successfully.
     */
    bool LoadAndExecute(TASProject *project);

    /**
     * @brief Stops script execution and cleans up.
     */
    void Stop();

    /**
     * @brief Processes one tick of script execution.
     * This advances the Lua scheduler and processes any pending script operations.
     */
    void Tick();

    /**
     * @brief Checks if a script is currently executing.
     * @return True if script execution is active.
     */
    bool IsExecuting() const;

    /**
     * @brief Gets the current script project being executed.
     * @return Pointer to the current project, or nullptr if none.
     */
    const TASProject *GetCurrentProject() const { return m_CurrentProject; }

    /**
     * @brief Gets the Lua state for direct access (used by LuaApi).
     * @return Reference to the Lua state.
     */
    sol::state &GetLuaState() { return m_LuaState; }

    /**
     * @brief Gets the Lua scheduler for coroutine management.
     * @return Pointer to the scheduler, or nullptr if not initialized.
     */
    LuaScheduler *GetScheduler() const { return m_Scheduler.get(); }

    /**
     * @brief Fires a game event to any listening Lua scripts.
     * @param eventName The name of the event.
     * @param args Optional arguments to pass to event handlers.
     */
    template <typename... Args>
    void FireGameEvent(const std::string &eventName, Args... args);

private:
    /**
     * @brief Prepares a project for execution by extracting it if needed.
     * @param project The project to prepare.
     * @return The execution path, or empty string on failure.
     */
    std::string PrepareProjectForExecution(TASProject *project);

    /**
     * @brief Cleans up any temporary resources for the current project.
     */
    void CleanupCurrentProject();

    // Core references
    TASEngine *m_Engine;

    // Lua execution environment
    sol::state m_LuaState;
    std::unique_ptr<LuaScheduler> m_Scheduler;

    // Current execution state
    TASProject *m_CurrentProject = nullptr;
    std::string m_CurrentExecutionPath;
    bool m_IsExecuting = false;
    bool m_IsInitialized = false;
};
