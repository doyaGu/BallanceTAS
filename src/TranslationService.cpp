/**
 * @file TranslationService.cpp
 * @brief Implementation of TranslationService - record-to-script translation lifecycle.
 */

#include "TranslationService.h"

#include "GameEvents.h"
#include "IGameControl.h"
#include "IInputAccess.h"
#include "InputSystem.h"
#include "Recorder.h"
#include "RecordPlayer.h"
#include "ScriptContext.h"
#include "ScriptContextManager.h"
#include "TASProject.h"
#include "ScriptGenerator.h"
#include "Logger.h"
#include "GameInterface.h"

#include <CKInputManager.h>
#include <CKTimeManager.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <utility>

namespace {

std::string ResolveTranslationLevelName(const TASProject *project, GameInterface &game) {
    if (!project) {
        return {};
    }

    return ScriptContextManager::ResolveLevelKey(
        project->GetTargetLevel(),
        game.GetMapName(),
        game.GetCurrentLevel());
}

std::string SanitizeFileStem(std::string name) {
    std::replace_if(name.begin(), name.end(),
                    [](char c) {
                        return c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' ||
                               c == '?' || c == '"' || c == '<' || c == '>' || c == '|';
                    },
                    '_');
    if (name.empty()) {
        return "Translated_Record";
    }
    return name;
}

} // namespace

TranslationService::TranslationService(Recorder &recorder,
                                       RecordPlayer &recordPlayer,
                                       IGameControl &gameControl,
                                       IInputAccess &inputAccess,
                                       GameInterface &gameInterface,
                                       ScriptContextManager &scriptContextManager,
                                       InputSystem &inputSystem,
                                       HookManager &hookManager,
                                       EventBus &eventBus,
                                       std::string tasRootPath)
    : m_EventBus(eventBus),
      m_HookManager(hookManager),
      m_Recorder(recorder),
      m_RecordPlayer(recordPlayer),
      m_GameControl(gameControl),
      m_InputAccess(inputAccess),
      m_GameInterface(gameInterface),
      m_ScriptContextManager(scriptContextManager),
      m_InputSystem(inputSystem),
      m_TasRootPath(std::move(tasRootPath)) {
}

TranslationService::~TranslationService() {
    if (m_IsTranslating) {
        StopTranslationImmediate();
    }
}

Result<void> TranslationService::PrepareTranslation(TASProject *project) {
    if (!project) {
        return Result<void>::Error("Project cannot be null", "invalid_argument");
    }
    if (m_IsTranslating || m_IsPrepared) {
        return Result<void>::Error("Already translating or prepared", "state");
    }

    if (project->IsRecordProject()) {
        if (!project->IsValid() || !project->CanBeTranslated()) {
            return Result<void>::Error(
                "Record cannot be translated: " + project->GetTranslationCompatibilityMessage(),
                "translation");
        }
        m_Direction = TranslationDirection::RecordToScript;
    } else if (project->IsScriptProject()) {
        if (!project->CanTranslateToRecord()) {
            return Result<void>::Error(project->GetScriptToRecordCompatibilityMessage(), "translation");
        }
        m_Direction = TranslationDirection::ScriptToRecord;
    } else {
        return Result<void>::Error("Unsupported project type for translation", "translation");
    }

    m_CurrentProject = project;
    m_IsPrepared = true;
    m_CompletionSignaled = false;
    m_CurrentTick = 0;
    m_ScriptContextName.clear();
    m_CapturedRecordFrames.clear();
    m_LastOutputPath.clear();
    m_LastResultMessage.clear();

    Log::Info("TranslationService: Prepared %s translation for '%s'.",
              m_Direction == TranslationDirection::RecordToScript ? "record-to-script" : "script-to-record",
              project->GetName().c_str());
    return Result<void>::Ok();
}

Result<void> TranslationService::ActivateTranslation() {
    if (!m_IsPrepared) {
        return Result<void>::Error("No prepared translation to activate", "state");
    }
    return m_Direction == TranslationDirection::ScriptToRecord
        ? ActivateScriptToRecordTranslation()
        : ActivateRecordToScriptTranslation();
}

Result<void> TranslationService::ActivateRecordToScriptTranslation() {
    auto options = BuildGenerationOptions(m_CurrentProject);
    m_Recorder.SetGenerationOptions(options);
    m_Recorder.SetUpdateRate(m_CurrentProject->GetUpdateRate());
    m_Recorder.SetAutoGenerate(true);
    m_Recorder.SetTranslationMode(true);

    m_Recorder.Start();
    if (!m_Recorder.IsRecording()) {
        return Result<void>::Error("Failed to start recorder for translation", "recording");
    }

    if (!m_RecordPlayer.LoadAndPlay(m_CurrentProject)) {
        m_Recorder.Stop();
        return Result<void>::Error("Failed to start record playback for translation", "playback");
    }

    m_CurrentTick = 0;
    m_CompletionSignaled = false;
    InstallRecordToScriptCallbacks();

    m_IsPrepared = false;
    m_IsTranslating = true;

    Log::Info("TranslationService: Activated translation for '%s'.",
              m_CurrentProject->GetName().c_str());
    return Result<void>::Ok();
}

Result<void> TranslationService::ActivateScriptToRecordTranslation() {
    if (!m_CurrentProject || !m_CurrentProject->CanTranslateToRecord()) {
        return Result<void>::Error(
            m_CurrentProject ? m_CurrentProject->GetScriptToRecordCompatibilityMessage() : "No project set",
            "translation");
    }

    m_GameControl.AcquireKeyBindings();

    auto ctx = m_ScriptContextManager.GetOrCreateLevelContext(
        ResolveTranslationLevelName(m_CurrentProject, m_GameInterface));
    if (!ctx) {
        return Result<void>::Error("Failed to create script context", "translation");
    }
    if (!ctx->LoadAndExecute(m_CurrentProject)) {
        return Result<void>::Error("Failed to load and execute script", "translation");
    }

    m_RecordInputMapping.keyUp = m_GameInterface.RemapKey(CKKEY_UP);
    m_RecordInputMapping.keyDown = m_GameInterface.RemapKey(CKKEY_DOWN);
    m_RecordInputMapping.keyLeft = m_GameInterface.RemapKey(CKKEY_LEFT);
    m_RecordInputMapping.keyRight = m_GameInterface.RemapKey(CKKEY_RIGHT);
    m_RecordInputMapping.keyShift = m_GameInterface.RemapKey(CKKEY_LSHIFT);
    m_RecordInputMapping.keySpace = m_GameInterface.RemapKey(CKKEY_SPACE);

    m_ScriptContextName = ctx->GetName();
    m_InputSystem.SetEnabled(true);
    m_InputSystem.Reset();
    m_CapturedRecordFrames.clear();
    m_CurrentTick = 0;
    m_CompletionSignaled = false;

    InstallScriptToRecordCallbacks();

    m_IsPrepared = false;
    m_IsTranslating = true;

    Log::Info("TranslationService: Activated script-to-record translation for '%s'.",
              m_CurrentProject->GetName().c_str());
    return Result<void>::Ok();
}

Result<void> TranslationService::StopTranslationGraceful(bool clearProject) {
    if (m_IsPrepared && !m_IsTranslating) {
        m_IsPrepared = false;
        m_CompletionSignaled = false;
        m_Direction = TranslationDirection::None;
        m_CapturedRecordFrames.clear();
        m_ScriptContextName.clear();
        if (clearProject) {
            m_CurrentProject = nullptr;
        }
        Log::Info("TranslationService: Cancelled prepared translation.");
        return Result<void>::Ok();
    }

    if (!m_IsTranslating) {
        return Result<void>::Error("Not translating", "state");
    }

    RemoveHookCallbacks();

    if (m_Direction == TranslationDirection::RecordToScript) {
        m_RecordPlayer.Stop();

        if (m_Recorder.IsRecording()) {
            m_Recorder.Stop();
            Log::Info("TranslationService: Recorder stopped, script generated.");
        }
    } else if (m_Direction == TranslationDirection::ScriptToRecord) {
        auto ctx = m_ScriptContextManager.GetContext(m_ScriptContextName);
        if (ctx && ctx->IsExecuting()) {
            ctx->Stop();
        }
        m_InputSystem.Reset();
        m_InputSystem.SetEnabled(false);
    }

    auto *im = m_InputAccess.GetInputManager();
    if (im) {
        memset(im->GetKeyboardState(), KS_IDLE, 256);
    }

    m_IsTranslating = false;
    m_IsPrepared = false;
    m_CompletionSignaled = false;
    m_Direction = TranslationDirection::None;
    m_CapturedRecordFrames.clear();
    m_ScriptContextName.clear();
    if (clearProject) {
        m_CurrentProject = nullptr;
    }

    Log::Info("TranslationService: Stopped translation.");
    return Result<void>::Ok();
}

void TranslationService::StopTranslationImmediate() {
    RemoveHookCallbacks();

    m_RecordPlayer.Stop();
    auto ctx = m_ScriptContextManager.GetContext(m_ScriptContextName);
    if (ctx && ctx->IsExecuting()) {
        ctx->Stop();
    }
    if (m_Recorder.IsRecording()) {
        m_Recorder.Stop();
    }
    m_InputSystem.Reset();
    m_InputSystem.SetEnabled(false);

    auto *im = m_InputAccess.GetInputManager();
    if (im) {
        memset(im->GetKeyboardState(), KS_IDLE, 256);
    }

    m_IsTranslating = false;
    m_IsPrepared = false;
    m_CompletionSignaled = false;
    m_Direction = TranslationDirection::None;
    m_CapturedRecordFrames.clear();
    m_ScriptContextName.clear();
    m_CurrentProject = nullptr;
}

float TranslationService::GetProgress() const {
    if (!m_IsTranslating) {
        return 0.0f;
    }

    if (m_Direction == TranslationDirection::RecordToScript) {
        size_t total = m_RecordPlayer.GetTotalFrames();
        return total > 0
            ? static_cast<float>(m_RecordPlayer.GetCurrentFrame()) / static_cast<float>(total)
            : 0.0f;
    }

    return 0.0f;
}

void TranslationService::InstallRecordToScriptCallbacks() {
    m_PostTickGuard = m_HookManager.RegisterPostTickCallback(
        [this](CKBaseManager *man) {
            if (!m_IsTranslating) {
                return;
            }

            auto *tm = static_cast<CKTimeManager *>(man);
            if (m_CurrentProject && m_CurrentProject->IsValid()) {
                tm->SetLastDeltaTime(m_CurrentProject->GetDeltaTime());
            }
        });

    m_PostInputGuard = m_HookManager.RegisterPostInputCallback(
        [this](CKBaseManager *man) {
            if (!m_IsTranslating) {
                return;
            }

            auto *im = static_cast<CKInputManager *>(man);
            unsigned char *keyboardState = im->GetKeyboardState();

            if (m_RecordPlayer.IsPlaying()) {
                m_RecordPlayer.Tick(m_CurrentTick, keyboardState);
            }

            if (m_Recorder.IsRecording()) {
                m_Recorder.Tick(m_CurrentTick, keyboardState);
            }

            ++m_CurrentTick;

            if (!m_CompletionSignaled && !m_RecordPlayer.IsPlaying()) {
                OnRecordToScriptPlaybackComplete();
            }
        });
}

void TranslationService::InstallScriptToRecordCallbacks() {
    m_PostTickGuard = m_HookManager.RegisterPostTickCallback(
        [this](CKBaseManager *man) {
            if (!m_IsTranslating) {
                return;
            }

            auto *tm = static_cast<CKTimeManager *>(man);
            if (m_CurrentProject && m_CurrentProject->IsValid()) {
                tm->SetLastDeltaTime(m_CurrentProject->GetDeltaTime());
            }
        });

    m_PostInputGuard = m_HookManager.RegisterPostInputCallback(
        [this](CKBaseManager *man) {
            if (!m_IsTranslating) {
                return;
            }

            auto *im = static_cast<CKInputManager *>(man);
            unsigned char *keyboardState = im->GetKeyboardState();

            m_ScriptContextManager.TickAll();
            auto inputs = GetActiveScriptInputs();
            InputSystem::ApplyMergedToKeyboardState(m_CurrentTick, inputs, keyboardState);

            if (m_CurrentProject) {
                m_CapturedRecordFrames.push_back(
                    tas::record::CaptureKeyboardStateToRecordFrame(
                        keyboardState,
                        m_RecordInputMapping,
                        m_CurrentProject->GetDeltaTime()));
            }

            ++m_CurrentTick;

            auto playbackContext = m_ScriptContextManager.GetContext(m_ScriptContextName);
            const bool playbackRunning = playbackContext && playbackContext->IsExecuting();
            if (!m_CompletionSignaled && !playbackRunning) {
                OnScriptToRecordPlaybackComplete();
            }
        });
}

void TranslationService::RemoveHookCallbacks() {
    m_PostTickGuard.Reset();
    m_PostInputGuard.Reset();
}

void TranslationService::OnRecordToScriptPlaybackComplete() {
    if (m_CompletionSignaled) {
        return;
    }

    m_CompletionSignaled = true;
    Log::Info("TranslationService: Record playback completed. Waiting for state-machine stop.");

    m_EventBus.Publish(TranslationCompletedEvent{});
}

void TranslationService::OnScriptToRecordPlaybackComplete() {
    if (m_CompletionSignaled) {
        return;
    }

    m_CompletionSignaled = true;

    const auto outputPath = BuildRecordOutputPath(m_CurrentProject);
    auto result = tas::record::WriteLegacyRecordFile(outputPath, m_CapturedRecordFrames);
    if (result.IsOk()) {
        m_LastOutputPath = outputPath.string();
        m_LastResultMessage = "Record written: " + outputPath.filename().string();
        Log::Info("TranslationService: Script-to-record completed: %s",
                  m_LastOutputPath.c_str());
    } else {
        m_LastOutputPath.clear();
        m_LastResultMessage = "Record translation failed: " + result.GetError().message;
        Log::Error("TranslationService: %s", m_LastResultMessage.c_str());
        std::filesystem::remove(outputPath);
    }

    m_EventBus.Publish(TranslationCompletedEvent{});
}

GenerationOptions TranslationService::BuildGenerationOptions(const TASProject *project) const {
    GenerationOptions options;
    options.projectName = project->GetName() + "_Script";
    options.authorName = project->GetAuthor();
    options.targetLevel = project->GetTargetLevel();
    options.description = "Translated from legacy record: " + project->GetName();
    options.updateRate = project->GetUpdateRate();
    options.addFrameComments = true;
    return options;
}

std::filesystem::path TranslationService::BuildRecordOutputPath(const TASProject *project) const {
    std::filesystem::path root = m_TasRootPath.empty()
        ? std::filesystem::current_path()
        : std::filesystem::path(m_TasRootPath);

    std::string stem = SanitizeFileStem(project ? project->GetName() : "Translated_Record");
    std::filesystem::path candidate = root / (stem + ".tas");
    for (int index = 1; std::filesystem::exists(candidate) && index < 1000; ++index) {
        candidate = root / (stem + "_" + std::to_string(index) + ".tas");
    }
    return candidate;
}

std::vector<InputSystem *> TranslationService::GetActiveScriptInputs() const {
    std::vector<InputSystem *> activeInputs;
    for (const auto &ctx : m_ScriptContextManager.GetContextsByPriority()) {
        if (!ctx || !ctx->IsExecuting()) {
            continue;
        }

        auto *inputSystem = const_cast<InputSystem *>(ctx->GetInputSystem());
        if (inputSystem && inputSystem->IsEnabled()) {
            activeInputs.push_back(inputSystem);
        }
    }
    return activeInputs;
}
