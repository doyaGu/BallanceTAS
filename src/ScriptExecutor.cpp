#include "ScriptExecutor.h"

#include "TASEngine.h"
#include "TASProject.h"
#include "LuaScheduler.h"
#include "LuaApi.h"
#include "EventManager.h"
#include "ProjectManager.h"

ScriptExecutor::ScriptExecutor(TASEngine *engine) : m_Engine(engine) {
    if (!m_Engine) {
        throw std::runtime_error("ScriptExecutor requires a valid TASEngine instance.");
    }
}

ScriptExecutor::~ScriptExecutor() {
    Shutdown();
}

bool ScriptExecutor::Initialize() {
    if (m_IsInitialized) {
        m_Engine->GetLogger()->Warn("ScriptExecutor already initialized.");
        return true;
    }

    m_Engine->GetLogger()->Info("Initializing ScriptExecutor...");

    try {
        // 1. Initialize Lua State
        m_LuaState.open_libraries(
            sol::lib::base,
            sol::lib::package,
            sol::lib::coroutine,
            sol::lib::string,
            sol::lib::os,
            sol::lib::math,
            sol::lib::table,
            sol::lib::debug,
            sol::lib::io // Potentially restrict this for security later
        );

        // 2. Create Lua Scheduler
        m_Scheduler = std::make_unique<LuaScheduler>(m_Engine);

        // 3. Register Lua APIs
        LuaApi::Register(m_Engine);

        m_IsInitialized = true;
        m_Engine->GetLogger()->Info("ScriptExecutor initialized successfully.");
        return true;
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Failed to initialize ScriptExecutor: %s", e.what());
        return false;
    }
}

void ScriptExecutor::Shutdown() {
    if (!m_IsInitialized) return;

    m_Engine->GetLogger()->Info("Shutting down ScriptExecutor...");

    try {
        // Stop any running script
        Stop();

        // Shutdown scheduler
        if (m_Scheduler) {
            m_Scheduler->Clear();
            m_Scheduler.reset();
        }

        // Clean up Lua state (automatic with sol2)
        m_LuaState = sol::state{};

        m_IsInitialized = false;
        m_Engine->GetLogger()->Info("ScriptExecutor shutdown complete.");
    } catch (const std::exception &e) {
        if (m_Engine && m_Engine && m_Engine->GetLogger()) {
            m_Engine->GetLogger()->Error("Exception during ScriptExecutor shutdown: %s", e.what());
        }
    }
}

bool ScriptExecutor::LoadAndExecute(TASProject *project) {
    if (!m_IsInitialized) {
        m_Engine->GetLogger()->Error("ScriptExecutor not initialized.");
        return false;
    }

    if (!project || !project->IsScriptProject() || !project->IsValid()) {
        m_Engine->GetLogger()->Error("Invalid script project provided to ScriptExecutor.");
        return false;
    }

    // Stop any currently running script
    Stop();

    try {
        // Prepare project for execution
        std::string executionPath = PrepareProjectForExecution(project);
        if (executionPath.empty()) {
            m_Engine->GetLogger()->Error("Failed to prepare script project for execution: %s", project->GetName().c_str());
            return false;
        }

        // Get the entry script path
        std::string entryScriptPath = project->GetEntryScriptPath(executionPath);
        if (entryScriptPath.empty()) {
            m_Engine->GetLogger()->Error("No entry script found for project: %s", project->GetName().c_str());
            return false;
        }

        m_Engine->GetLogger()->Info("Loading TAS script: %s", entryScriptPath.c_str());

        // Load and execute the main script file in the Lua VM
        auto result = m_LuaState.safe_script_file(entryScriptPath, &sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            m_Engine->GetLogger()->Error("Failed to execute script: %s", err.what());
            CleanupCurrentProject();
            return false;
        }

        // The script should define a global 'main' function
        sol::function mainFunc = m_LuaState["main"];
        if (!mainFunc.valid()) {
            m_Engine->GetLogger()->Error("'main' function not found in entry script.");
            CleanupCurrentProject();
            return false;
        }

        // Start the main coroutine
        if (m_Scheduler) {
            m_Scheduler->AddCoroutineTask(mainFunc);
        }

        // Set execution state
        m_CurrentProject = project;
        m_CurrentExecutionPath = executionPath;
        m_IsExecuting = true;

        m_Engine->GetLogger()->Info("TAS script '%s' loaded and started.", project->GetName().c_str());
        return true;
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Exception loading TAS script: %s", e.what());
        CleanupCurrentProject();
        return false;
    }
}

void ScriptExecutor::Stop() {
    if (!m_IsExecuting) return;

    m_Engine->GetLogger()->Info("Stopping script execution...");

    try {
        // Clear scheduler
        if (m_Scheduler) {
            m_Scheduler->Clear();
        }

        // Clean up project resources
        CleanupCurrentProject();

        // Reset state
        m_IsExecuting = false;
        m_CurrentProject = nullptr;
        m_CurrentExecutionPath.clear();

        m_Engine->GetLogger()->Info("Script execution stopped.");
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Exception stopping script execution: %s", e.what());
    }
}

void ScriptExecutor::Tick() {
    if (!m_IsExecuting || !m_Scheduler) {
        return;
    }

    try {
        // Process Lua scheduler
        m_Scheduler->Tick();

        // Check if script execution has completed
        if (!m_Scheduler->IsRunning()) {
            m_Engine->GetLogger()->Info("Script execution completed naturally.");
            Stop();
        }
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Exception during script tick: %s", e.what());
        Stop(); // Stop on error to prevent further issues
    }
}

bool ScriptExecutor::IsExecuting() const {
    return m_IsExecuting && m_Scheduler && m_Scheduler->IsRunning();
}

template <typename... Args>
void ScriptExecutor::FireGameEvent(const std::string &eventName, Args... args) {
    if (!m_IsExecuting || !m_Engine) {
        return;
    }

    try {
        auto *eventManager = m_Engine->GetEventManager();
        if (eventManager) {
            eventManager->FireEvent(eventName, args...);
        }
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Exception firing game event to script: %s", e.what());
    }
}

// Explicit template instantiations for events used in TASEngine
template void ScriptExecutor::FireGameEvent(const std::string &);
template void ScriptExecutor::FireGameEvent(const std::string &, int);

std::string ScriptExecutor::PrepareProjectForExecution(TASProject *project) {
    if (!project || !project->IsScriptProject()) {
        return "";
    }

    // For zip projects, we need to prepare them for execution (extract if needed)
    if (project->IsZipProject()) {
        auto *projectManager = m_Engine->GetProjectManager();
        if (!projectManager) {
            m_Engine->GetLogger()->Error("ProjectManager not available for zip project preparation.");
            return "";
        }

        std::string executionPath = projectManager->PrepareProjectForExecution(const_cast<TASProject *>(project));
        if (executionPath.empty()) {
            m_Engine->GetLogger()->Error("Failed to prepare zip project for execution: %s",
                                                   project->GetName().c_str());
            return "";
        }

        // Update the project's execution base path for script resolution
        const_cast<TASProject *>(project)->SetExecutionBasePath(executionPath);

        m_Engine->GetLogger()->Info("Zip project prepared for execution: %s -> %s",
                                              project->GetPath().c_str(), executionPath.c_str());
        return executionPath;
    } else {
        // For directory projects, use the project path directly
        return project->GetPath();
    }
}

void ScriptExecutor::CleanupCurrentProject() {
    if (!m_CurrentProject) {
        return;
    }

    // Clean up temporary directories for zip projects
    if (m_CurrentProject->IsZipProject() && m_Engine->GetProjectManager()) {
        m_Engine->GetProjectManager()->CleanupProjectTempDirectory(m_CurrentProject);
        m_CurrentProject->SetExecutionBasePath(""); // Clear execution base path
    }

    m_CurrentProject = nullptr;
    m_CurrentExecutionPath.clear();
}
