#pragma once

#include <sol/sol.hpp>
#include <string>
#include <memory>
#include <functional>

#include "ThreadOwnershipValidator.h"

// Forward declarations
class TASEngine;
class TASProject;
class LuaScheduler;
class EventManager;
class ProjectManager;
class InputSystem;
class RecordPlayer;
class GameInterface;
class ScriptContextManager;

/**
 * @brief Type of script context
 */
enum class ScriptContextType {
    Global, // Global context that persists across levels
    Level,  // Level-specific context
    Custom  // User-created context
};

/**
 * @brief Lua GC mode (Lua 5.4+)
 */
enum class LuaGCMode {
    Generational, // Generational GC (default for TAS, better for short-burst workloads)
    Incremental   // Incremental GC (better for long-lived scripts with timely finalization)
};

/**
 * @class ScriptContext
 * @brief An independent script execution context with its own Lua VM and scheduler.
 *
 * ScriptContext is designed to be nestable and isolated. Each context has:
 * - Its own Lua VM (sol::state)
 * - Its own LuaScheduler for coroutine management
 * - Its own EventManager for event handling
 * - Its own execution state and priority
 *
 * This allows multiple scripts to run simultaneously without interfering with each other.
 */
class ScriptContext {
public:
    /**
     * @brief Constructs a new script context.
     * @param engine The TASEngine instance.
     * @param name The unique name of this context (e.g., "global", "level_01").
     * @param type The type of this context.
     * @param priority The priority for input and event handling (higher = more priority).
     */
    explicit ScriptContext(TASEngine *engine, std::string name,
                           ScriptContextType type = ScriptContextType::Custom,
                           int priority = 0);
    ~ScriptContext();

    // ScriptContext is not copyable or movable
    ScriptContext(const ScriptContext &) = delete;
    ScriptContext &operator=(const ScriptContext &) = delete;

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
     * @brief Reinitializes the context for reuse from the context pool.
     * Resets all execution state while preserving the expensive Lua VM and registered APIs.
     * @param newName The new name for this context.
     * @param newPriority The new priority for this context.
     * @return True if reinitialization was successful.
     */
    bool Reinitialize(const std::string &newName, int newPriority);

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
     * @brief Gets the context name.
     * @return The unique name of this context.
     */
    const std::string &GetName() const { return m_Name; }

    /**
     * @brief Gets the context type.
     * @return The type of this context.
     */
    ScriptContextType GetType() const { return m_Type; }

    /**
     * @brief Gets the context priority.
     * @return The priority of this context.
     */
    int GetPriority() const { return m_Priority; }

    /**
     * @brief Sets the context priority.
     * @param priority The new priority value.
     */
    void SetPriority(int priority) { m_Priority = priority; }

    /**
     * @brief Gets the ScriptContextManager that owns this context.
     * @return Pointer to the owning ScriptContextManager.
     */
    ScriptContextManager *GetScriptContextManager() const;

    /**
     * @brief Gets the current tick/frame number of execution.
     * @return The current tick number.
     */
    size_t GetCurrentTick() const;

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
     * @brief Gets the event manager for this context.
     * @return Pointer to the event manager, or nullptr if not initialized.
     */
    EventManager *GetEventManager() const { return m_EventManager.get(); }

    /**
     * @brief Gets the project manager from the engine.
     * @return Pointer to the ProjectManager.
     */
    ProjectManager *GetProjectManager() const;

    /**
     * @brief Gets the input system for this context.
     * @return Pointer to the input system, or nullptr if not initialized.
     */
    InputSystem *GetInputSystem() const { return m_InputSystem.get(); }

    /**
     * @brief Gets the record player associated with the engine.
     * @return Pointer to the RecordPlayer, or nullptr if not available.
     */
    RecordPlayer *GetRecordPlayer() const;

    /**
     * @brief Gets the game interface associated with the engine.
     * @return Pointer to the GameInterface, or nullptr if not available.
     */
    GameInterface *GetGameInterface() const;

    /**
     * @brief Sets a callback to be called when execution status changes.
     * @param callback Function called with true when starting, false when stopping.
     */
    void SetStatusCallback(std::function<void(bool)> callback) {
        m_StatusCallback = std::move(callback);
    }

    // --- GC Mode Management ---

    /**
     * @brief Sets the Lua GC mode for this context.
     * @param mode The GC mode to use (Generational or Incremental).
     * @return True if the mode was set successfully.
     */
    bool SetGCMode(LuaGCMode mode);

    /**
     * @brief Gets the current Lua GC mode.
     * @return The current GC mode.
     */
    LuaGCMode GetGCMode() const { return m_GCMode; }

    // --- Memory Monitoring ---

    /**
     * @brief Gets the current Lua memory usage in bytes.
     * @return Memory usage in bytes (via collectgarbage("count") * 1024).
     */
    size_t GetLuaMemoryBytes() const;

    /**
     * @brief Gets the Lua memory usage in kilobytes.
     * @return Memory usage in KB.
     */
    double GetLuaMemoryKB() const;

    // --- Sleep/Idle Management ---

    /**
     * @brief Checks if the context is currently sleeping (idle).
     * @return True if the context is sleeping.
     */
    bool IsSleeping() const { return m_Sleeping; }

    /**
     * @brief Checks if the context should go to sleep (no active tasks).
     * @return True if context has no scheduled coroutines, empty inbox, and no timers.
     */
    bool ShouldSleep() const;

    /**
     * @brief Wakes up the context from sleep.
     */
    void WakeUp() { m_Sleeping = false; }

    /**
     * @brief Puts the context to sleep if it should sleep.
     */
    void TrySleep();

    /**
     * @brief Gets the sleep interval (frames between ticks when sleeping).
     * @return Number of frames to skip when sleeping.
     */
    int GetSleepInterval() const { return m_SleepInterval; }

    /**
     * @brief Sets the sleep interval.
     * @param interval Number of frames to skip when sleeping (e.g., 8).
     */
    void SetSleepInterval(int interval) { m_SleepInterval = interval; }

    /**
     * @brief Fires a game event to any listening Lua scripts in this context.
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

    /**
     * @brief Notifies status change via callback.
     * @param isExecuting True if starting execution, false if stopping.
     */
    void NotifyStatusChange(bool isExecuting) const {
        if (m_StatusCallback) {
            m_StatusCallback(isExecuting);
        }
    }

    // Core references
    TASEngine *m_Engine;

    // Context identity
    std::string m_Name;
    ScriptContextType m_Type;
    int m_Priority;

    // Lua execution environment (isolated)
    sol::state m_LuaState;
    std::unique_ptr<LuaScheduler> m_Scheduler;
    std::unique_ptr<EventManager> m_EventManager;
    std::unique_ptr<InputSystem> m_InputSystem;

    // Current execution state
    TASProject *m_CurrentProject = nullptr;
    std::string m_CurrentExecutionPath;
    bool m_IsExecuting = false;
    bool m_IsInitialized = false;

    // Callback for execution status changes
    std::function<void(bool)> m_StatusCallback;

    // GC management
    LuaGCMode m_GCMode = LuaGCMode::Generational; // Default to generational for TAS workloads

    // Sleep/idle management
    bool m_Sleeping = false;
    int m_SleepInterval = 8;        // Skip 8 frames when sleeping (configurable)
    int m_TicksSinceLastActive = 0; // Counter for sleep detection

    // Thread safety enforcement
    mutable ThreadOwnershipValidator m_ThreadValidator{"ScriptContext"};
};
