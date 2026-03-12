/**
 * @file TranslationService.cpp
 * @brief Implementation of TranslationService - record-to-script translation lifecycle.
 */

#include "TranslationService.h"

#include "EventBus.h"
#include "GameEvents.h"
#include "ServiceContainer.h"
#include "Recorder.h"
#include "RecordPlayer.h"
#include "GameInterface.h"
#include "TASProject.h"
#include "ScriptGenerator.h"
#include "Logger.h"

#include <CKInputManager.h>
#include <CKTimeManager.h>
#include <cstring>

TranslationService::TranslationService(ServiceProvider *provider)
    : m_ServiceProvider(provider) {
    if (!m_ServiceProvider) {
        throw std::invalid_argument("ServiceProvider cannot be null");
    }

    m_Recorder = m_ServiceProvider->Resolve<Recorder>();
    m_RecordPlayer = m_ServiceProvider->Resolve<RecordPlayer>();
    m_GameInterface = m_ServiceProvider->Resolve<GameInterface>();
}

TranslationService::~TranslationService() {
    if (m_IsTranslating) {
        StopTranslationImmediate();
    }
}

void TranslationService::SetEventBus(EventBus *bus) {
    m_EventBus = bus;
}

void TranslationService::SetHookManager(HookManager *hookMgr) {
    m_HookManager = hookMgr;
}

Result<void> TranslationService::PrepareTranslation(TASProject *project) {
    if (!project) {
        return Result<void>::Error("Project cannot be null", "invalid_argument");
    }
    if (!project->IsRecordProject() || !project->IsValid()) {
        return Result<void>::Error("Translation requires a valid record project", "invalid_argument");
    }
    if (!project->CanBeTranslated()) {
        return Result<void>::Error(
            "Record cannot be translated: " + project->GetTranslationCompatibilityMessage(),
            "translation");
    }
    if (m_IsTranslating || m_IsPrepared) {
        return Result<void>::Error("Already translating or prepared", "state");
    }

    m_CurrentProject = project;
    m_IsPrepared = true;
    m_CompletionSignaled = false;

    Log::Info("TranslationService: Prepared translation for '%s'.", project->GetName().c_str());
    return Result<void>::Ok();
}

Result<void> TranslationService::ActivateTranslation() {
    if (!m_IsPrepared) {
        return Result<void>::Error("No prepared translation to activate", "state");
    }
    if (!m_Recorder || !m_RecordPlayer) {
        return Result<void>::Error("Recorder or RecordPlayer not available", "subsystem");
    }

    auto options = BuildGenerationOptions(m_CurrentProject);
    m_Recorder->SetGenerationOptions(options);
    m_Recorder->SetUpdateRate(m_CurrentProject->GetUpdateRate());
    m_Recorder->SetAutoGenerate(true);
    m_Recorder->SetTranslationMode(true);

    m_Recorder->Start();
    if (!m_Recorder->IsRecording()) {
        return Result<void>::Error("Failed to start recorder for translation", "recording");
    }

    if (!m_RecordPlayer->LoadAndPlay(m_CurrentProject)) {
        m_Recorder->Stop();
        return Result<void>::Error("Failed to start record playback for translation", "playback");
    }

    m_CurrentTick = 0;
    m_CompletionSignaled = false;
    InstallCallbacks();

    m_IsPrepared = false;
    m_IsTranslating = true;

    Log::Info("TranslationService: Activated translation for '%s'.",
              m_CurrentProject->GetName().c_str());
    return Result<void>::Ok();
}

Result<void> TranslationService::StopTranslationGraceful(bool clearProject) {
    if (m_IsPrepared && !m_IsTranslating) {
        m_IsPrepared = false;
        m_CompletionSignaled = false;
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

    if (m_RecordPlayer) {
        m_RecordPlayer->Stop();
    }

    if (m_Recorder && m_Recorder->IsRecording()) {
        m_Recorder->Stop();
        Log::Info("TranslationService: Recorder stopped, script generated.");
    }

    if (m_GameInterface) {
        auto *im = m_GameInterface->GetInputManager();
        if (im) {
            memset(im->GetKeyboardState(), KS_IDLE, 256);
        }
    }

    m_IsTranslating = false;
    m_IsPrepared = false;
    m_CompletionSignaled = false;
    if (clearProject) {
        m_CurrentProject = nullptr;
    }

    Log::Info("TranslationService: Stopped translation.");
    return Result<void>::Ok();
}

void TranslationService::StopTranslationImmediate() {
    RemoveHookCallbacks();

    if (m_RecordPlayer) {
        m_RecordPlayer->Stop();
    }
    if (m_Recorder && m_Recorder->IsRecording()) {
        m_Recorder->Stop();
    }

    if (m_GameInterface) {
        auto *im = m_GameInterface->GetInputManager();
        if (im) {
            memset(im->GetKeyboardState(), KS_IDLE, 256);
        }
    }

    m_IsTranslating = false;
    m_IsPrepared = false;
    m_CompletionSignaled = false;
    m_CurrentProject = nullptr;
}

float TranslationService::GetProgress() const {
    if (!m_IsTranslating || !m_RecordPlayer) {
        return 0.0f;
    }

    size_t total = m_RecordPlayer->GetTotalFrames();
    return total > 0
        ? static_cast<float>(m_RecordPlayer->GetCurrentFrame()) / static_cast<float>(total)
        : 0.0f;
}

void TranslationService::InstallCallbacks() {
    if (!m_HookManager) {
        return;
    }

    m_PostTickGuard = m_HookManager->RegisterPostTickCallback(
        [this](CKBaseManager *man) {
            if (!m_IsTranslating) {
                return;
            }

            auto *tm = static_cast<CKTimeManager *>(man);
            if (m_CurrentProject && m_CurrentProject->IsValid()) {
                tm->SetLastDeltaTime(m_CurrentProject->GetDeltaTime());
            }
        });

    m_PostInputGuard = m_HookManager->RegisterPostInputCallback(
        [this](CKBaseManager *man) {
            if (!m_IsTranslating) {
                return;
            }

            auto *im = static_cast<CKInputManager *>(man);
            unsigned char *keyboardState = im->GetKeyboardState();

            if (m_RecordPlayer && m_RecordPlayer->IsPlaying()) {
                m_RecordPlayer->Tick(m_CurrentTick, keyboardState);
            }

            if (m_Recorder && m_Recorder->IsRecording()) {
                m_Recorder->Tick(m_CurrentTick, keyboardState);
            }

            ++m_CurrentTick;

            if (!m_CompletionSignaled && m_RecordPlayer && !m_RecordPlayer->IsPlaying()) {
                OnPlaybackComplete();
            }
        });
}

void TranslationService::RemoveHookCallbacks() {
    m_PostTickGuard.Reset();
    m_PostInputGuard.Reset();
}

void TranslationService::OnPlaybackComplete() {
    if (m_CompletionSignaled) {
        return;
    }

    m_CompletionSignaled = true;
    Log::Info("TranslationService: Record playback completed. Waiting for state-machine stop.");

    if (m_EventBus) {
        m_EventBus->Publish(TranslationCompletedEvent{});
    }
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
