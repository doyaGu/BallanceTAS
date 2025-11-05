#include "ScriptContext.h"

#include "Logger.h"
#include "TASEngine.h"
#include "TASProject.h"
#include "LuaScheduler.h"
#include "LuaApi.h"
#include "EventManager.h"
#include "InputSystem.h"
#include "ProjectManager.h"
#include "ScriptContextManager.h"
#include "MessageBus.h"
#include "SharedDataManager.h"

ScriptContext::ScriptContext(TASEngine *engine, std::string name, ScriptContextType type, int priority)
    : m_Engine(engine), m_Name(std::move(name)), m_Type(type), m_Priority(priority) {
    if (!m_Engine) {
        throw std::runtime_error("ScriptContext requires a valid TASEngine instance.");
    }
    if (m_Name.empty()) {
        throw std::runtime_error("ScriptContext requires a non-empty name.");
    }
}

ScriptContext::~ScriptContext() {
    Shutdown();
}

bool ScriptContext::Initialize() {
    m_ThreadValidator.AssertOwnership();

    if (m_IsInitialized) {
        Log::Warn("[%s] ScriptContext already initialized.", m_Name.c_str());
        return true;
    }

    Log::Info("[%s] Initializing ScriptContext...", m_Name.c_str());

    try {
        // 1. Initialize Lua State (independent VM for this context)
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

        // 2. Create Lua Scheduler (independent scheduler for this context)
        // Pass 'this' context for proper context isolation
        m_Scheduler = std::make_unique<LuaScheduler>(m_Engine, this);

        // 3. Create Event Manager (independent event manager for this context)
        m_EventManager = std::make_unique<EventManager>();

        // 4. Create Input System (independent input system for this context)
        m_InputSystem = std::make_unique<InputSystem>();

        // 5. Register Lua APIs for this context (multi-context mode)
        // Uses context's local subsystems (InputSystem, Scheduler, EventManager)
        LuaApi::Register(this);

        LuaApi::AddLuaPath(GetLuaState(), BML_TAS_PATH);
        LuaApi::AddLuaPath(GetLuaState(), BML_TAS_PATH "lua");

        // 6. Set default GC mode (Generational for TAS workloads)
        SetGCMode(LuaGCMode::Generational);

        m_IsInitialized = true;
        Log::Info("[%s] ScriptContext initialized successfully.", m_Name.c_str());
        return true;
    } catch (const std::exception &e) {
        Log::Error("[%s] Failed to initialize ScriptContext: %s", m_Name.c_str(), e.what());
        return false;
    }
}

void ScriptContext::Shutdown() {
    m_ThreadValidator.AssertOwnership();

    if (!m_IsInitialized) return;

    if (m_Engine == nullptr) {
        Log::Error("[%s] Cannot shutdown ScriptContext: TASEngine instance is null.", m_Name.c_str());
        return;
    }

    Log::Info("[%s] Shutting down ScriptContext...", m_Name.c_str());

    try {
        // Clean up inter-context communication registrations
        auto *contextManager = m_Engine->GetScriptContextManager();
        if (contextManager) {
            // Remove all message handlers for this context
            auto *messageBus = contextManager->GetMessageBus();
            if (messageBus) {
                messageBus->RemoveAllHandlers(m_Name);
            }

            // Remove all shared data watches for this context
            auto *sharedData = contextManager->GetSharedData();
            if (sharedData) {
                sharedData->UnwatchAll(m_Name);
            }
        }

        // Stop any running script
        Stop();

        // Shutdown input system
        if (m_InputSystem) {
            m_InputSystem->Reset();
            m_InputSystem.reset();
        }

        // Shutdown event manager
        if (m_EventManager) {
            m_EventManager->ClearListeners();
            m_EventManager.reset();
        }

        // Shutdown scheduler
        if (m_Scheduler) {
            m_Scheduler->Clear();
            m_Scheduler.reset();
        }

        // Mark as uninitialized before destroying Lua state
        // This prevents any code from trying to use the context during Lua state destruction
        m_IsInitialized = false;

        // Clean up Lua state (automatic with sol2)
        m_LuaState = sol::state{};

        Log::Info("[%s] ScriptContext shutdown complete.", m_Name.c_str());
    } catch (const std::exception &e) {
        if (m_Engine) {
            Log::Error("[%s] Exception during ScriptContext shutdown: %s", m_Name.c_str(), e.what());
        }
    }
}

bool ScriptContext::Reinitialize(const std::string &newName, int newPriority) {
    m_ThreadValidator.AssertOwnership();

    if (!m_IsInitialized) {
        Log::Error("[%s] Cannot reinitialize an uninitialized ScriptContext.", m_Name.c_str());
        return false;
    }

    Log::Info("[%s] Reinitializing ScriptContext as '%s' with priority %d...",
              m_Name.c_str(), newName.c_str(), newPriority);

    try {
        // 1. Clean up inter-context communication registrations
        auto *contextManager = m_Engine->GetScriptContextManager();
        if (contextManager) {
            // Remove all message handlers for this context
            auto *messageBus = contextManager->GetMessageBus();
            if (messageBus) {
                messageBus->RemoveAllHandlers(m_Name);
            }

            // Remove all shared data watches for this context
            auto *sharedData = contextManager->GetSharedData();
            if (sharedData) {
                sharedData->UnwatchAll(m_Name);
            }
        }

        // 2. Stop any running script execution
        Stop();

        // 3. Clear scheduler tasks (but keep the scheduler object)
        if (m_Scheduler) {
            m_Scheduler->Clear();
        }

        // 4. Clear event listeners (but keep the event manager object)
        if (m_EventManager) {
            m_EventManager->ClearListeners();
        }

        // 5. Reset input system (but keep the input system object)
        if (m_InputSystem) {
            m_InputSystem->Reset();
        }

        // 6. Reset sleep/idle state
        m_Sleeping = false;
        m_TicksSinceLastActive = 0;

        // 7. Force Lua garbage collection to clean up previous script's memory
        lua_State *L = m_LuaState.lua_state();
        if (L) {
            lua_gc(L, LUA_GCCOLLECT, 0); // Full GC cycle
        }

        // 8. Update context identity
        m_Name = newName;
        m_Priority = newPriority;

        // Note: We preserve m_LuaState (expensive VM), registered APIs, GC mode, and m_IsInitialized

        Log::Info("[%s] ScriptContext reinitialized successfully.", m_Name.c_str());
        return true;
    } catch (const std::exception &e) {
        Log::Error("[%s] Failed to reinitialize ScriptContext: %s",
                   m_Name.c_str(), e.what());
        return false;
    }
}

bool ScriptContext::LoadAndExecute(TASProject *project) {
    m_ThreadValidator.AssertOwnership();

    if (!m_IsInitialized) {
        Log::Error("[%s] ScriptContext not initialized.", m_Name.c_str());
        return false;
    }

    if (!project || !project->IsScriptProject() || !project->IsValid()) {
        Log::Error("[%s] Invalid script project provided to ScriptContext.", m_Name.c_str());
        return false;
    }

    // Stop any currently running script
    Stop();

    try {
        // Prepare project for execution
        std::string executionPath = PrepareProjectForExecution(project);
        if (executionPath.empty()) {
            Log::Error("[%s] Failed to prepare script project for execution: %s",
                       m_Name.c_str(), project->GetName().c_str());
            return false;
        }

        // Get the entry script path
        std::string entryScriptPath = project->GetEntryScriptPath(executionPath);
        if (entryScriptPath.empty()) {
            Log::Error("[%s] No entry script found for project: %s",
                       m_Name.c_str(), project->GetName().c_str());
            return false;
        }

        Log::Info("[%s] Loading TAS script: %s",
                  m_Name.c_str(), entryScriptPath.c_str());

        // Load and execute the main script file in the Lua VM
        auto result = m_LuaState.safe_script_file(entryScriptPath, &sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            Log::Error("[%s] Failed to execute script: %s",
                       m_Name.c_str(), err.what());
            CleanupCurrentProject();
            return false;
        }

        // The script should define a global 'main' function
        sol::function mainFunc = m_LuaState["main"];
        if (!mainFunc.valid()) {
            Log::Error("[%s] 'main' function not found in entry script.",
                       m_Name.c_str());
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

        NotifyStatusChange(true);

        Log::Info("[%s] TAS script '%s' loaded and started.",
                  m_Name.c_str(), project->GetName().c_str());
        return true;
    } catch (const std::exception &e) {
        Log::Error("[%s] Exception loading TAS script: %s",
                   m_Name.c_str(), e.what());
        CleanupCurrentProject();
        return false;
    }
}

void ScriptContext::Stop() {
    m_ThreadValidator.AssertOwnership();

    if (!m_IsExecuting) return;

    Log::Info("[%s] Stopping script execution...", m_Name.c_str());

    try {
        // Clear scheduler
        if (m_Scheduler) {
            m_Scheduler->Clear();
        }

        // Clear event listeners
        if (m_EventManager) {
            m_EventManager->ClearListeners();
        }

        // Clean up project resources
        CleanupCurrentProject();
        m_CurrentProject = nullptr;
        m_CurrentExecutionPath.clear();

        // Reset execution state
        m_IsExecuting = false;

        NotifyStatusChange(false);

        Log::Info("[%s] Script execution stopped.", m_Name.c_str());
    } catch (const std::exception &e) {
        Log::Error("[%s] Exception stopping script execution: %s", m_Name.c_str(), e.what());
    }
}

void ScriptContext::Tick() {
    m_ThreadValidator.AssertOwnership();

    if (!m_IsExecuting || !m_Scheduler) {
        return;
    }

    // Handle sleep mode: only tick every N frames when sleeping
    if (m_Sleeping) {
        m_TicksSinceLastActive++;
        if (m_TicksSinceLastActive < m_SleepInterval) {
            return; // Skip this tick, still sleeping
        }
        // Time for a sleep-tick, reset counter
        m_TicksSinceLastActive = 0;
    }

    try {
        // Process Lua scheduler
        m_Scheduler->Tick();

        // Check if script execution has completed
        if (!m_Scheduler->IsRunning()) {
            Log::Info("[%s] Script execution completed naturally.", m_Name.c_str());
            m_IsExecuting = false;
            NotifyStatusChange(false);
        } else {
            // Check if context should go to sleep after this tick
            TrySleep();
        }
    } catch (const std::exception &e) {
        Log::Error("[%s] Exception during script tick: %s", m_Name.c_str(), e.what());
        m_IsExecuting = false;
        NotifyStatusChange(false);
    }
}

bool ScriptContext::IsExecuting() const {
    return m_IsExecuting && m_Scheduler && m_Scheduler->IsRunning();
}

ScriptContextManager *ScriptContext::GetScriptContextManager() const {
    return m_Engine->GetScriptContextManager();
}

size_t ScriptContext::GetCurrentTick() const {
    return m_Engine->GetCurrentTick();
}

template <typename... Args>
void ScriptContext::FireGameEvent(const std::string &eventName, Args... args) {
    if (!m_IsExecuting || !m_EventManager) {
        return;
    }

    try {
        m_EventManager->FireEvent(eventName, args...);
    } catch (const std::exception &e) {
        Log::Error("[%s] Exception firing game event to script: %s", m_Name.c_str(), e.what());
    }
}

// Explicit template instantiations for events used in TASEngine
template void ScriptContext::FireGameEvent(const std::string &);
template void ScriptContext::FireGameEvent(const std::string &, int);

std::string ScriptContext::PrepareProjectForExecution(TASProject *project) {
    if (!project || !project->IsScriptProject()) {
        return "";
    }

    // For zip projects, we need to prepare them for execution (extract if needed)
    if (project->IsZipProject()) {
        auto *projectManager = m_Engine->GetProjectManager();
        if (!projectManager) {
            Log::Error("[%s] ProjectManager not available for zip project preparation.",
                       m_Name.c_str());
            return "";
        }

        std::string executionPath = projectManager->PrepareProjectForExecution(project);
        if (executionPath.empty()) {
            Log::Error("[%s] Failed to prepare zip project for execution: %s",
                       m_Name.c_str(), project->GetName().c_str());
            return "";
        }

        // Update the project's execution base path for script resolution
        project->SetExecutionBasePath(executionPath);

        Log::Info("[%s] Zip project prepared for execution: %s -> %s",
                  m_Name.c_str(), project->GetPath().c_str(), executionPath.c_str());
        return executionPath;
    } else {
        // For directory projects, use the project path directly
        return project->GetPath();
    }
}

void ScriptContext::CleanupCurrentProject() {
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

ProjectManager *ScriptContext::GetProjectManager() const {
    return m_Engine->GetProjectManager();
}

RecordPlayer *ScriptContext::GetRecordPlayer() const {
    return m_Engine->GetRecordPlayer();
}

GameInterface *ScriptContext::GetGameInterface() const {
    return m_Engine->GetGameInterface();
}

// ============================================================================
// GC Mode Management
// ============================================================================

bool ScriptContext::SetGCMode(LuaGCMode mode) {
    if (!m_IsInitialized) {
        Log::Warn("[%s] Cannot set GC mode: context not initialized.", m_Name.c_str());
        return false;
    }

    try {
        lua_State *L = m_LuaState.lua_state();

        // STACK SAFETY: lua_gc() does not manipulate the Lua stack, so no stack guard needed.
        // However, we record the stack top for debug validation.
#ifdef _DEBUG
        int stackTop = lua_gettop(L);
#endif

        // Lua 5.4+ GC modes: LUA_GCGEN (generational), LUA_GCINC (incremental)
        // Note: Check Lua version and availability
#if LUA_VERSION_NUM >= 504
        if (mode == LuaGCMode::Generational) {
            // Switch to generational mode
            lua_gc(L, LUA_GCGEN, 0, 0);
            m_GCMode = LuaGCMode::Generational;
            Log::Info("[%s] GC mode set to Generational.", m_Name.c_str());
        } else {
            // Switch to incremental mode
            lua_gc(L, LUA_GCINC, 0, 0, 0);
            m_GCMode = LuaGCMode::Incremental;
            Log::Info("[%s] GC mode set to Incremental.", m_Name.c_str());
        }

        // DEBUG: Verify stack balance
#ifdef _DEBUG
        int stackTopAfter = lua_gettop(L);
        if (stackTop != stackTopAfter) {
            Log::Error("[%s] STACK IMBALANCE in SetGCMode: before=%d, after=%d",
                       m_Name.c_str(), stackTop, stackTopAfter);
        }
#endif

        return true;
#else
        // Lua 5.3 or earlier - only incremental GC available
        m_GCMode = LuaGCMode::Incremental;
        Log::Warn("[%s] Lua version < 5.4: only incremental GC available.", m_Name.c_str());

        // DEBUG: Verify stack balance
#ifdef _DEBUG
        int stackTopAfter = lua_gettop(L);
        if (stackTop != stackTopAfter) {
            Log::Error("[%s] STACK IMBALANCE in SetGCMode: before=%d, after=%d",
                       m_Name.c_str(), stackTop, stackTopAfter);
        }
#endif

        return false;
#endif
    } catch (const std::exception &e) {
        Log::Error("[%s] Failed to set GC mode: %s", m_Name.c_str(), e.what());
        // NOTE: If an exception occurs, the stack should still be balanced since lua_gc()
        // doesn't push/pop values. But we log the error to be safe.
        return false;
    }
}

// ============================================================================
// Memory Monitoring
// ============================================================================

size_t ScriptContext::GetLuaMemoryBytes() const {
    if (!m_IsInitialized) {
        return 0;
    }

    try {
        // Get memory usage via collectgarbage("count") which returns KB
        lua_State *L = m_LuaState.lua_state();
        int kb = lua_gc(L, LUA_GCCOUNT, 0);
        return static_cast<size_t>(kb) * 1024;
    } catch (const std::exception &) {
        return 0;
    }
}

double ScriptContext::GetLuaMemoryKB() const {
    if (!m_IsInitialized) {
        return 0.0;
    }

    try {
        lua_State *L = m_LuaState.lua_state();
        int kb = lua_gc(L, LUA_GCCOUNT, 0);
        return static_cast<double>(kb);
    } catch (const std::exception &) {
        return 0.0;
    }
}

// ============================================================================
// Sleep/Idle Management
// ============================================================================

bool ScriptContext::ShouldSleep() const {
    if (!m_IsExecuting) {
        return true; // Not executing, can sleep
    }

    // Check if scheduler has active tasks
    if (m_Scheduler && m_Scheduler->IsRunning()) {
        return false; // Has active coroutines, don't sleep
    }

    // Note: Message delivery and event callbacks wake the context by scheduling work
    // through the scheduler, so checking scheduler state is sufficient for sleep detection.
    // The ScriptContextManager is responsible for waking sleeping contexts when:
    // - New messages arrive (delivered via scheduler)
    // - Events are fired (callbacks scheduled via scheduler)
    // - Shared data watches trigger (callbacks scheduled via scheduler)

    // If no active tasks, can sleep
    return true;
}

void ScriptContext::TrySleep() {
    if (ShouldSleep() && !m_Sleeping) {
        m_Sleeping = true;
        m_TicksSinceLastActive = 0;
        Log::Info("[%s] Context entering sleep mode.", m_Name.c_str());
    }
}
