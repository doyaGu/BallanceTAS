#pragma once

#include "Result.h"
#include "HookManager.h"
#include "EventBus.h"
#include "RecordFileIO.h"

#include <filesystem>
#include <string>
#include <vector>

// Forward declarations
class TASProject;
class Recorder;
class RecordPlayer;
class IGameControl;
class IInputAccess;
class GameInterface;
class InputSystem;
class ProjectManager;
class ScriptContextManager;
struct GenerationOptions;

enum class TranslationDirection {
    None,
    RecordToScript,
    ScriptToRecord,
};

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
    TranslationService(Recorder &recorder,
                       RecordPlayer &recordPlayer,
                       IGameControl &gameControl,
                       IInputAccess &inputAccess,
                       GameInterface &gameInterface,
                       ScriptContextManager &scriptContextManager,
                       InputSystem &inputSystem,
                       HookManager &hookManager,
                       EventBus &eventBus,
                       std::string tasRootPath);
    ~TranslationService();

    TranslationService(const TranslationService &) = delete;
    TranslationService &operator=(const TranslationService &) = delete;

    /**
     * @brief Validate the project and mark translation as pending.
     * @param project  A valid record project (.tas).
     * @return Error if the project isn't a translatable record.
     */
    Result<void> PrepareTranslation(TASProject *project);

    /**
     * @brief Activate translation when level loading starts.
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
    TranslationDirection GetDirection() const { return m_Direction; }
    float GetProgress() const;
    size_t GetCurrentTick() const { return m_CurrentTick; }
    const std::string &GetLastOutputPath() const { return m_LastOutputPath; }
    const std::string &GetLastResultMessage() const { return m_LastResultMessage; }

private:
    Result<void> ActivateRecordToScriptTranslation();
    Result<void> ActivateScriptToRecordTranslation();
    void InstallRecordToScriptCallbacks();
    void InstallScriptToRecordCallbacks();
    void RemoveHookCallbacks();
    void OnRecordToScriptPlaybackComplete();
    void OnScriptToRecordPlaybackComplete();
    GenerationOptions BuildGenerationOptions(const TASProject *project) const;
    std::filesystem::path BuildRecordOutputPath(const TASProject *project) const;
    std::vector<InputSystem *> GetActiveScriptInputs() const;

    EventBus &m_EventBus;
    HookManager &m_HookManager;
    Recorder &m_Recorder;
    RecordPlayer &m_RecordPlayer;
    IGameControl &m_GameControl;
    IInputAccess &m_InputAccess;
    GameInterface &m_GameInterface;
    ScriptContextManager &m_ScriptContextManager;
    InputSystem &m_InputSystem;
    std::string m_TasRootPath;

    // State
    TASProject *m_CurrentProject = nullptr;
    TranslationDirection m_Direction = TranslationDirection::None;
    bool m_IsPrepared = false;
    bool m_IsTranslating = false;
    bool m_CompletionSignaled = false;
    size_t m_CurrentTick = 0;
    std::string m_ScriptContextName;
    tas::record::RecordInputMapping m_RecordInputMapping;
    std::vector<RecordFrameData> m_CapturedRecordFrames;
    std::string m_LastOutputPath;
    std::string m_LastResultMessage;

    // RAII hook guards
    ScopedCallback m_PostTickGuard;
    ScopedCallback m_PostInputGuard;
};
