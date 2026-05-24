#pragma once

#include <string>

#include "GameEvents.h"

class IGameQuery;
class ScriptContextManager;

class ContextLifecycleCoordinator {
public:
    ContextLifecycleCoordinator(ScriptContextManager &scriptContextManager, IGameQuery &gameQuery);

    void EnsureGlobalContext();
    void EnsureLevelContext();
    void DestroyLevelContexts();
    void OnStartLevel(const StartLevelEvent &event);
    void OnPostStartMenu(const PostStartMenuEvent &event);
    void OnPostExitLevel(const PostExitLevelEvent &event);
    void OnGameOver(const GameOverEvent &event);

private:
    std::string GetCurrentLevelName() const;

    ScriptContextManager &m_ScriptContextManager;
    IGameQuery &m_GameQuery;
};
