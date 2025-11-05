#pragma once

#include <memory>
#include <string>
#include <vector>
#include "TASProject.h"

class TASEngine;

/**
 * @class StartupProjectManager
 * @brief Manages startup script loading and execution for whole-process gameplay.
 *
 * This component handles the selection, loading, and execution of global TAS projects
 * that can run across menus and multiple levels. It integrates with the TASEngine
 * to provide seamless startup script functionality.
 */
class StartupProjectManager {
public:
    explicit StartupProjectManager(TASEngine *engine);
    ~StartupProjectManager();

    // Non-copyable, non-movable
    StartupProjectManager(const StartupProjectManager &) = delete;
    StartupProjectManager &operator=(const StartupProjectManager &) = delete;

    /**
     * @brief Initializes the startup project manager.
     * @return True if initialization succeeded, false otherwise.
     */
    bool Initialize();

    /**
     * @brief Shuts down the startup project manager.
     */
    void Shutdown();

    /**
     * @brief Sets the startup project by name.
     * @param projectName The name of the project to set as startup.
     * @return True if the project was found and set, false otherwise.
     */
    bool SetStartupProject(const std::string &projectName);

    /**
     * @brief Gets the current startup project.
     * @return Pointer to the current startup project, or nullptr if none is set.
     */
    TASProject *GetStartupProject() const { return m_StartupProject.get(); }

    /**
     * @brief Checks if a startup project is configured and ready.
     * @return True if a startup project is available and ready to execute.
     */
    bool HasStartupProject() const { return m_StartupProject && m_StartupProject->IsReadyForExecution(); }

    /**
     * @brief Checks if startup script execution is enabled.
     * @return True if startup scripts are enabled.
     */
    bool IsStartupEnabled() const { return m_StartupEnabled; }

    /**
     * @brief Enables or disables startup script functionality.
     * @param enabled True to enable startup scripts, false to disable.
     */
    void SetStartupEnabled(bool enabled) { m_StartupEnabled = enabled; }

    /**
     * @brief Gets the configured startup project name.
     * @return The configured startup project name.
     */
    const std::string &GetStartupProjectName() const { return m_StartupProjectName; }

    /**
     * @brief Checks if auto-loading is enabled.
     * @return True if auto-loading startup scripts is enabled.
     */
    bool IsAutoLoadEnabled() const { return m_AutoLoadEnabled; }

    /**
     * @brief Enables or disables auto-loading of startup scripts.
     * @param enabled True to enable auto-loading, false to disable.
     */
    void SetAutoLoadEnabled(bool enabled) { m_AutoLoadEnabled = enabled; }

    /**
     * @brief Attempts to load and execute the startup script.
     * @return True if the startup script was successfully loaded and started, false otherwise.
     */
    bool LoadAndExecuteStartupScript();

    /**
     * @brief Should be called when game starts to trigger startup script execution.
     */
    void OnGameStart();

    /**
     * @brief Should be called when entering main menu.
     */
    void OnEnterMainMenu();

    /**
     * @brief Should be called when entering a level.
     * @param levelName The name of the level being entered.
     */
    void OnEnterLevel(const std::string &levelName);

    /**
     * @brief Refreshes the startup project from the configured name.
     * Call this when the project list might have changed.
     * @return True if the startup project was successfully refreshed.
     */
    bool RefreshStartupProject();

    /**
     * @brief Gets a list of all available global projects.
     * @return Vector of global project names.
     */
    std::vector<std::string> GetAvailableGlobalProjects() const;

private:
    /**
     * @brief Loads the startup project from the configured name.
     * @return True if the project was successfully loaded, false otherwise.
     */
    bool LoadStartupProject();

    /**
     * @brief Executes the current startup project if it's appropriate for the current context.
     * @param context The current execution context ("startup", "menu", or "level").
     * @param levelName The current level name (only used for "level" context).
     * @return True if the startup project was executed, false otherwise.
     */
    bool ExecuteStartupProjectIfAppropriate(const std::string &context, const std::string &levelName = "");

    TASEngine *m_Engine;
    std::unique_ptr<TASProject> m_StartupProject;
    std::string m_StartupProjectName;
    bool m_StartupEnabled = false;
    bool m_AutoLoadEnabled = false;
    bool m_Initialized = false;
    bool m_HasExecutedStartup = false; // Prevent multiple executions
};