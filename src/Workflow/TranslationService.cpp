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
#include "ScriptFrameTime.h"
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

static std::string ResolveTranslationLevelName(const TASProject *project, GameInterface &game) {
    if (!project) {
        return {};
    }

    return ScriptContextManager::ResolveLevelKey(
        project->GetTargetLevel(),
        game.GetMapName(),
        game.GetCurrentLevel());
}

static std::string SanitizeFileStem(std::string name) {
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

TranslationService::TranslationService(Recorder &recorder,
                                       RecordPlayer &recordPlayer,
                                       IGameControl &gameControl,
                                       IInputAccess &inputAccess,
                                       GameInterface &gameInterface,
                                       ScriptContextManager &scriptContextManager,
                                       InputSystem &inputSystem,
                                       HookManager &hookManager,
                                       EventBus &eventBus,
                                       ScriptFrameTime &frameTime,
                                       std::string tasRootPath)
    : m_EventBus(eventBus),
      m_FrameTime(frameTime),
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
    m_Recorder.SetAutoGenerate(false);
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
    m_FrameTime.Begin(m_CurrentProject->GetInitialFrameTime());
    if (!ctx->LoadAndExecute(m_CurrentProject)) {
        m_FrameTime.End();
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
    m_RecordWritten = false;
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
    m_FrameTime.End();

    if (m_Direction == TranslationDirection::RecordToScript) {
        m_RecordPlayer.Stop();

        if (m_Recorder.IsRecording()) {
            m_Recorder.Stop();
        }
        FinishRecordToScript();
    } else if (m_Direction == TranslationDirection::ScriptToRecord) {
        auto ctx = m_ScriptContextManager.GetContext(m_ScriptContextName);
        if (ctx && ctx->IsExecuting()) {
            ctx->Stop();
        }
        m_InputSystem.Reset();
        m_InputSystem.SetEnabled(false);
        // The level end can stop the translation before the script returns.
        WriteCapturedRecord();
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
    m_FrameTime.End();

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

            // Each record frame's own time, exactly as record playback gives it.
            auto *tm = static_cast<CKTimeManager *>(man);
            tm->SetLastDeltaTime(m_RecordPlayer.GetFrameDeltaTime(m_CurrentTick));
        });

    m_PostInputGuard = m_HookManager.RegisterPostInputCallback(
        [this](CKBaseManager *man) {
            if (!m_IsTranslating) {
                return;
            }

            auto *im = static_cast<CKInputManager *>(man);
            unsigned char *keyboardState = im->GetKeyboardState();

            // The player notices the record's end one tick after its last frame; that tick
            // is not a record frame, so the script must not get it.
            const bool recordFrame = m_RecordPlayer.IsPlaying() && m_CurrentTick < m_RecordPlayer.GetTotalFrames();
            const float recordFrameTime = recordFrame ? m_RecordPlayer.GetFrameDeltaTimeByFrame(m_CurrentTick) : 0.0f;
            if (m_RecordPlayer.IsPlaying()) {
                m_RecordPlayer.Tick(m_CurrentTick, keyboardState);
            }

            if (recordFrame && m_Recorder.IsRecording()) {
                m_Recorder.Tick(m_CurrentTick, keyboardState);
                // The generated script reproduces the record's frame times.
                m_Recorder.SetLastFrameDeltaTime(recordFrameTime);
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
            tm->SetLastDeltaTime(m_FrameTime.Get());
        });

    m_PostInputGuard = m_HookManager.RegisterPostInputCallback(
        [this](CKBaseManager *man) {
            if (!m_IsTranslating) {
                return;
            }

            auto *im = static_cast<CKInputManager *>(man);
            unsigned char *keyboardState = im->GetKeyboardState();

            // This frame runs at the time in effect before the script's tick; a change the
            // tick makes applies from the next frame.
            const float frameTime = m_FrameTime.Get();
            m_ScriptContextManager.TickAll();

            // The tick a script returns in carries none of its keys, so it is not one of
            // the script's frames.
            auto playbackContext = m_ScriptContextManager.GetContext(m_ScriptContextName);
            const bool playbackRunning = playbackContext && playbackContext->IsExecuting();
            if (playbackRunning) {
                auto inputs = GetActiveScriptInputs();
                InputSystem::ApplyMergedToKeyboardState(m_CurrentTick, inputs, keyboardState);
                m_CapturedRecordFrames.push_back(
                    tas::record::CaptureKeyboardStateToRecordFrame(
                        keyboardState,
                        m_RecordInputMapping,
                        frameTime));
            }

            ++m_CurrentTick;

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
    WriteCapturedRecord();
    m_EventBus.Publish(TranslationCompletedEvent{});
}

void TranslationService::SetResult(bool ok, std::string outputPath, std::string message) {
    m_LastSucceeded = ok;
    m_LastOutputPath = ok ? std::move(outputPath) : std::string();
    m_LastResultMessage = std::move(message);
    ++m_ResultSerial;
    if (ok) {
        Log::Info("TranslationService: %s", m_LastResultMessage.c_str());
    } else {
        Log::Error("TranslationService: %s", m_LastResultMessage.c_str());
    }
}

void TranslationService::WriteCapturedRecord() {
    if (m_RecordWritten) {
        return;
    }
    m_RecordWritten = true;
    if (m_CapturedRecordFrames.empty()) {
        SetResult(false, {}, "Record translation failed: the script ran no frames");
        return;
    }

    const auto outputPath = BuildRecordOutputPath(m_CurrentProject);
    auto result = tas::record::WriteLegacyRecordFile(outputPath, m_CapturedRecordFrames);
    if (result.IsOk()) {
        SetResult(true, outputPath.string(), "Record written: " + outputPath.filename().string());
    } else {
        std::filesystem::remove(outputPath);
        SetResult(false, {}, "Record translation failed: " + result.GetError().message);
    }
}

void TranslationService::FinishRecordToScript() {
    const bool ok = !m_Recorder.GetFrames().empty() && m_Recorder.GenerateScript(false);
    const std::string path = ok ? m_Recorder.GetLastGeneratedPath() : std::string();
    // Leave the recorder as plain recording expects it.
    m_Recorder.SetTranslationMode(false);
    m_Recorder.SetAutoGenerate(true);
    if (ok && !path.empty()) {
        SetResult(true, path, "Script written: " + std::filesystem::path(path).filename().string());
    } else {
        SetResult(false, {}, "Script translation failed: no script was generated");
    }
}

GenerationOptions TranslationService::BuildGenerationOptions(const TASProject *project) const {
    GenerationOptions options;
    options.projectName = project->GetName() + "_Script";
    options.authorName = project->GetAuthor();
    options.targetLevel = project->GetTargetLevel();
    options.description = "Translated from legacy record: " + project->GetName();
    options.updateRate = project->GetUpdateRate();
    options.frameTime = project->GetInitialFrameTime();
    options.addFrameComments = true;
    return options;
}

std::filesystem::path TranslationService::BuildRecordOutputPath(const TASProject *project) const {
    std::filesystem::path root = m_TasRootPath.empty()
        ? std::filesystem::current_path()
        : std::filesystem::path(m_TasRootPath);

    // "_Record" keeps the record's project name apart from the script it came from.
    std::string stem = SanitizeFileStem(project ? project->GetName() + "_Record" : "Translated_Record");
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
