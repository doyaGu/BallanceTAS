#pragma once

#include "Result.h"
#include "HookManager.h"
#include "EventBus.h"

#include <string>

// Forward declarations
class ServiceProvider;
class TASProject;
class Recorder;
class RecordPlayer;
class GameInterface;
class ProjectManager;
struct GenerationOptions;
struct StartLevelEvent;

/**
 * @class TranslationService
 * @brief Owns the full record-to-script translation lifecycle.
 *
 * Replaces: TranslationController.
 *
 * Translation plays back a record while simultaneously recording into the
 * Recorder (with auto-generate enabled), so the Recorder produces a Lua script
 * at the end.  The PostInput callback interleaves RecordPlayer tick → Recorder
 * tick each frame.
 *
 * Uses HookManager for per-frame callbacks (RAII-guarded). The state machine owns
 * pending/active truth and drives prepare/activate/stop.
 */
class TranslationService {
public:
    explicit TranslationService(ServiceProvider *provider);
    ~TranslationService();

    TranslationService(const TranslationService &) = delete;
    TranslationService &operator=(const TranslationService &) = delete;

    void SetEventBus(EventBus *bus);
    void SetHookManager(HookManager *hookMgr);

    /**
     * @brief Validate the project and mark translation as pending.
     * @param project  A valid record project (.tas).
     * @return Error if the project isn't a translatable record.
     */
    Result<void> PrepareTranslation(TASProject *project);

    /**
     * @brief Activate translation once the level actually loads.
     *
     * Configures the Recorder for auto-generation, starts RecordPlayer, and
     * installs per-frame callbacks.
     */
    Result<void> ActivateTranslation();

    /**
     * @brief Stop translation gracefully (Recorder auto-generates script on stop).
     * @param clearProject  If true, clears the project reference.
     */
    Result<void> StopTranslationGraceful(bool clearProject = true);

    /** Immediately stop without callbacks (for shutdown paths). */
    void StopTranslationImmediate();

    // --- Queries ---
    bool IsPrepared() const { return m_IsPrepared; }
    bool IsTranslating() const { return m_IsTranslating; }
    float GetProgress() const;
    size_t GetCurrentTick() const { return m_CurrentTick; }

private:
    void InstallCallbacks();
    void RemoveHookCallbacks();
    void OnPlaybackComplete();
    GenerationOptions BuildGenerationOptions(const TASProject *project) const;

    ServiceProvider *m_ServiceProvider;
    EventBus *m_EventBus = nullptr;
    HookManager *m_HookManager = nullptr;

    // Cached subsystems
    Recorder *m_Recorder = nullptr;
    RecordPlayer *m_RecordPlayer = nullptr;
    GameInterface *m_GameInterface = nullptr;

    // State
    TASProject *m_CurrentProject = nullptr;
    bool m_IsPrepared = false;
    bool m_IsTranslating = false;
    bool m_CompletionSignaled = false;
    size_t m_CurrentTick = 0;

    // RAII hook guards
    ScopedCallback m_PostTickGuard;
    ScopedCallback m_PostInputGuard;
};
