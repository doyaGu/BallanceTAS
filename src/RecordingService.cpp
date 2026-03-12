/**
 * @file RecordingService.cpp
 * @brief Implementation of RecordingService - full recording lifecycle.
 */

#include "RecordingService.h"

#include "EventBus.h"
#include "ServiceContainer.h"
#include "Recorder.h"
#include "InputSystem.h"
#include "GameInterface.h"
#include "Logger.h"

#include <CKInputManager.h>
#include <CKTimeManager.h>

RecordingService::RecordingService(ServiceProvider *provider)
    : m_ServiceProvider(provider) {
    if (!m_ServiceProvider) {
        throw std::invalid_argument("ServiceProvider cannot be null");
    }

    m_Recorder = m_ServiceProvider->Resolve<Recorder>();
    m_InputSystem = m_ServiceProvider->Resolve<InputSystem>();
    m_GameInterface = m_ServiceProvider->Resolve<GameInterface>();
}

RecordingService::~RecordingService() {
    if (m_IsRecording) {
        StopRecordingImmediate();
    }
}

void RecordingService::SetEventBus(EventBus *bus) {
    m_EventBus = bus;
}

void RecordingService::SetHookManager(HookManager *hookMgr) {
    m_HookManager = hookMgr;
}

Result<void> RecordingService::PrepareRecording(bool useValidation) {
    if (m_IsRecording) {
        return Result<void>::Error("Already recording", "recording_service");
    }
    if (m_IsPrepared) {
        return Result<void>::Error("Recording already prepared", "recording_service");
    }
    if (!m_Recorder) {
        return Result<void>::Error("Recorder subsystem not available", "recording_service");
    }

    m_UseValidation = useValidation;
    m_IsPrepared = true;
    m_CurrentTick = 0;

    Log::Info("RecordingService: Recording prepared.");
    return Result<void>::Ok();
}

Result<void> RecordingService::ActivateRecording() {
    if (m_IsRecording) {
        return Result<void>::Error("Already recording", "recording_service");
    }
    if (!m_IsPrepared) {
        return Result<void>::Error("Recording is not prepared", "recording_service");
    }
    if (!m_Recorder) {
        return Result<void>::Error("Recorder subsystem not available", "recording_service");
    }

    if (m_GameInterface) {
        m_GameInterface->AcquireKeyBindings();
    }

    SetupInputSystem();
    m_Recorder->Start();
    InstallHookCallbacks();

    m_IsPrepared = false;
    m_IsRecording = true;
    m_CurrentTick = 0;

    Log::Info("RecordingService: Recording activated%s.",
              m_UseValidation ? " (validation)" : "");
    return Result<void>::Ok();
}

Result<RecordingResult> RecordingService::StopRecordingGraceful() {
    if (!m_IsRecording && !m_IsPrepared) {
        return Result<RecordingResult>::Error("Not recording", "recording_service");
    }

    if (m_IsPrepared && !m_IsRecording) {
        m_IsPrepared = false;
        m_UseValidation = false;
        Log::Info("RecordingService: Cancelled prepared recording.");
        return Result<RecordingResult>::Ok(RecordingResult{{}, 0});
    }

    RemoveHookCallbacks();

    std::vector<FrameData> frames;
    if (m_Recorder) {
        frames = m_Recorder->Stop();
    }

    CleanupInputSystem();

    m_IsRecording = false;
    m_IsPrepared = false;
    m_UseValidation = false;

    RecordingResult result;
    result.frames = std::move(frames);
    result.totalFrames = result.frames.size();

    Log::Info("RecordingService: Stopped recording - captured %zu frames.",
              result.totalFrames);
    return Result<RecordingResult>::Ok(std::move(result));
}

void RecordingService::StopRecordingImmediate() {
    RemoveHookCallbacks();

    if (m_Recorder && m_Recorder->IsRecording()) {
        m_Recorder->SetAutoGenerate(false);
        m_Recorder->Stop();
    }

    CleanupInputSystem();

    m_IsRecording = false;
    m_IsPrepared = false;
    m_UseValidation = false;
}

size_t RecordingService::GetFrameCount() const {
    return m_Recorder ? m_Recorder->GetTotalFrames() : 0;
}

void RecordingService::InstallHookCallbacks() {
    if (!m_HookManager) {
        Log::Warn("RecordingService: No HookManager - callbacks not installed.");
        return;
    }

    m_PostTickGuard = m_HookManager->RegisterPostTickCallback(
        [this](CKTimeManager *tm) {
            if (!m_IsRecording || !m_Recorder) {
                return;
            }
            tm->SetLastDeltaTime(m_Recorder->GetDeltaTime());
        });

    m_PostInputGuard = m_HookManager->RegisterPostInputCallback(
        [this](CKInputManager *im) {
            if (!m_IsRecording || !m_Recorder) {
                return;
            }
            m_Recorder->Tick(m_CurrentTick, im->GetKeyboardState());
            ++m_CurrentTick;
        });
}

void RecordingService::RemoveHookCallbacks() {
    m_PostTickGuard.Reset();
    m_PostInputGuard.Reset();
}

void RecordingService::SetupInputSystem() {
    if (m_InputSystem) {
        m_InputSystem->Reset();
        m_InputSystem->SetEnabled(false);
    }
}

void RecordingService::CleanupInputSystem() {
    if (m_InputSystem) {
        m_InputSystem->Reset();
        m_InputSystem->SetEnabled(false);
    }
}
