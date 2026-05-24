#include "ContextLifecycleCoordinator.h"

#include "IGameQuery.h"
#include "Logger.h"
#include "ScriptContextManager.h"

ContextLifecycleCoordinator::ContextLifecycleCoordinator(
    ScriptContextManager &scriptContextManager,
    IGameQuery &gameQuery)
    : m_ScriptContextManager(scriptContextManager),
      m_GameQuery(gameQuery) {
}

void ContextLifecycleCoordinator::EnsureGlobalContext() {
    if (!m_ScriptContextManager.GetOrCreateGlobalContext()) {
        Log::Error("Failed to create global script context.");
    }
}

void ContextLifecycleCoordinator::EnsureLevelContext() {
    const std::string levelName = GetCurrentLevelName();
    if (levelName.empty()) {
        return;
    }

    if (!m_ScriptContextManager.GetOrCreateLevelContext(levelName)) {
        Log::Error("Failed to create level script context for '%s'.", levelName.c_str());
    }
}

void ContextLifecycleCoordinator::DestroyLevelContexts() {
    m_ScriptContextManager.DestroyAllLevelContexts();
}

void ContextLifecycleCoordinator::OnStartLevel(const StartLevelEvent &) {
    EnsureLevelContext();
}

void ContextLifecycleCoordinator::OnPostStartMenu(const PostStartMenuEvent &) {
    EnsureGlobalContext();
}

void ContextLifecycleCoordinator::OnPostExitLevel(const PostExitLevelEvent &) {
    DestroyLevelContexts();
}

void ContextLifecycleCoordinator::OnGameOver(const GameOverEvent &) {
    DestroyLevelContexts();
}

std::string ContextLifecycleCoordinator::GetCurrentLevelName() const {
    return ScriptContextManager::ResolveLevelKey("", m_GameQuery.GetMapName(), m_GameQuery.GetCurrentLevel());
}
