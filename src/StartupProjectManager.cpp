#include "StartupProjectManager.h"

#include "Logger.h"
#include "TASEngine.h"
#include "TASProject.h"
#include "ProjectManager.h"
#include "ScriptContext.h"
#include "ScriptContextManager.h"
#include "ServiceContainer.h"

StartupProjectManager::StartupProjectManager(TASEngine *engine) : m_Engine(engine) {}

StartupProjectManager::~StartupProjectManager() {
    Shutdown();
}

bool StartupProjectManager::Initialize() {
    if (m_Initialized) {
        return true;
    }

    if (!m_Engine) {
        return false;
    }

    m_Initialized = true;
    return true;
}

void StartupProjectManager::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    m_StartupProject.reset();
    m_StartupProjectName.clear();
    m_HasExecutedStartup = false;
    m_Initialized = false;
}

bool StartupProjectManager::SetStartupProject(const std::string &projectName) {
    if (!m_Initialized) {
        return false;
    }

    if (m_StartupProjectName != projectName) {
        m_StartupProjectName = projectName;
        m_HasExecutedStartup = false; // Reset execution flag when project changes
        return RefreshStartupProject();
    }

    return HasStartupProject();
}


bool StartupProjectManager::LoadAndExecuteStartupScript() {
    if (!m_Initialized || !m_StartupEnabled || !m_AutoLoadEnabled) {
        return false;
    }

    return LoadStartupProject() && ExecuteStartupProjectIfAppropriate("startup");
}

void StartupProjectManager::OnGameStart() {
    if (!m_Initialized || !m_StartupEnabled) {
        return;
    }

    // For startup-triggered projects
    if (m_AutoLoadEnabled && !m_HasExecutedStartup) {
        LoadAndExecuteStartupScript();
    }
}

void StartupProjectManager::OnEnterMainMenu() {
    if (!m_Initialized || !m_StartupEnabled) {
        return;
    }

    // Execute projects that should trigger on menu entry
    if (LoadProjectForContext("menu")) {
        ExecuteStartupProjectIfAppropriate("menu");
    }
}

void StartupProjectManager::OnEnterLevel(const std::string &levelName) {
    if (!m_Initialized || !m_StartupEnabled) {
        return;
    }

    // Execute projects that should trigger on level entry
    // This allows global projects to work in specific levels too
    if (LoadProjectForContext("level", levelName)) {
        ExecuteStartupProjectIfAppropriate("level", levelName);
    }
}

bool StartupProjectManager::RefreshStartupProject() {
    if (!m_Initialized) {
        m_StartupProject.reset();
        return false;
    }

    if (m_StartupProjectName.empty()) {
        m_StartupProject.reset();
        return false;
    }

    auto *projectManager = m_Engine->GetServiceProvider().Resolve<ProjectManager>();
    if (!projectManager) {
        m_StartupProject.reset();
        return false;
    }

    const auto &allProjects = projectManager->GetProjects();
    for (const auto &project : allProjects) {
        if (project && project->GetName() == m_StartupProjectName &&
            project->IsScriptProject() && project->IsGlobalProject() && project->IsValid()) {
            m_StartupProject = std::make_unique<TASProject>(*project);
            return true;
        }
    }

    m_StartupProject.reset();
    return false;
}

std::vector<std::string> StartupProjectManager::GetAvailableGlobalProjects() const {
    std::vector<std::string> globalProjects;

    if (!m_Initialized || !m_Engine) {
        return globalProjects;
    }

    auto *projectManager = m_Engine->GetServiceProvider().Resolve<ProjectManager>();
    if (!projectManager) {
        return globalProjects;
    }

    // Get all projects and filter for global ones
    const auto &allProjects = projectManager->GetProjects();
    for (const auto &project : allProjects) {
        if (project && project->IsGlobalProject() && project->IsValid()) {
            globalProjects.push_back(project->GetName());
        }
    }

    return globalProjects;
}

bool StartupProjectManager::LoadStartupProject() {
    return LoadProjectForContext("startup");
}

bool StartupProjectManager::LoadProjectForContext(const std::string &context, const std::string &levelName) {
    if (!m_Initialized) {
        return false;
    }

    auto *projectManager = m_Engine->GetServiceProvider().Resolve<ProjectManager>();
    if (!projectManager) {
        return false;
    }

    const auto &allProjects = projectManager->GetProjects();
    if (m_StartupProjectName.empty() && context != "startup") {
        m_StartupProject.reset();
        return false;
    }

    TASProject *selected = SelectProjectForContext(allProjects, m_StartupProjectName, context, levelName);
    if (!selected) {
        m_StartupProject.reset();
        return false;
    }

    m_StartupProject = std::make_unique<TASProject>(*selected);
    return true;
}

bool StartupProjectManager::ExecuteStartupProjectIfAppropriate(const std::string &context, const std::string &levelName) {
    if (!m_Initialized || !m_StartupEnabled || !m_StartupProject) {
        return false;
    }

    if (!IsProjectEligibleForContext(*m_StartupProject, context, levelName)) {
        return false;
    }

    auto *contextManager = m_Engine->GetServiceProvider().Resolve<ScriptContextManager>();
    if (!contextManager) {
        Log::Error("ScriptContextManager not available for startup project execution.");
        return false;
    }

    try {
        // Get or create the global context
        auto globalContext = contextManager->GetOrCreateGlobalContext();
        if (!globalContext) {
            Log::Error("Failed to get global context for startup project.");
            return false;
        }

        // Load and execute the startup project in the global context
        bool success = globalContext->LoadAndExecute(m_StartupProject.get());
        if (success) {
            m_HasExecutedStartup = true;
            Log::Info("Successfully loaded startup project '%s' into global context.",
                      m_StartupProject->GetName().c_str());
            return true;
        } else {
            Log::Error("Failed to load startup project '%s' into global context.",
                       m_StartupProject->GetName().c_str());
        }
    } catch (const std::exception &e) {
        // Log error but don't crash
        Log::Error("Exception while executing startup project '%s': %s",
                   m_StartupProject->GetName().c_str(), e.what());
    }

    return false;
}
