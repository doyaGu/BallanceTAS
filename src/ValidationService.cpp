/**
 * @file ValidationService.cpp
 * @brief Implementation of ValidationService - validation recording lifecycle.
 */

#include "ValidationService.h"

#include "EventBus.h"
#include "GameEvents.h"
#include "ServiceContainer.h"
#include "Recorder.h"
#include "PlaybackService.h"
#include "Logger.h"

#include <ctime>

ValidationService::ValidationService(ServiceProvider *provider)
    : m_ServiceProvider(provider) {
    if (!m_ServiceProvider) {
        throw std::invalid_argument("ServiceProvider cannot be null");
    }
    m_Recorder = m_ServiceProvider->Resolve<Recorder>();
}

ValidationService::~ValidationService() {
    if (m_IsActive) {
        StopImmediate();
    }
}

Result<void> ValidationService::Start(const std::string &outputPath,
                                      const PlaybackService &playback) {
    if (!playback.IsPlaying() || playback.GetPlaybackType() != PlaybackType::Script) {
        return Result<void>::Error(
            "Validation recording requires active script playback", "state");
    }
    if (!m_Recorder) {
        return Result<void>::Error("Recorder subsystem not available", "subsystem");
    }
    if (m_Recorder->IsRecording()) {
        return Result<void>::Error(
            "Cannot start validation while another recording is active", "state");
    }

    m_OutputPath = outputPath;

    m_Recorder->SetAutoGenerate(false);
    m_Recorder->ClearFrameData();
    m_Recorder->Start();

    m_IsActive = true;
    Log::Info("ValidationService: Started validation recording - output: %s",
              outputPath.c_str());
    if (m_EventBus) {
        m_EventBus->Publish(ValidationStartedEvent{outputPath});
    }
    return Result<void>::Ok();
}

Result<void> ValidationService::Stop() {
    if (!m_IsActive) {
        return Result<void>::Error("Validation recording is not active", "state");
    }

    if (!m_Recorder || !m_Recorder->IsRecording()) {
        m_IsActive = false;
        return Result<void>::Error("Recorder state inconsistent", "state");
    }

    auto frameData = m_Recorder->Stop();

    std::string path = m_OutputPath + "validation_" +
        std::to_string(std::time(nullptr)) + ".txt";

    bool ok = m_Recorder->DumpFrameData(path, true);
    if (ok) {
        Log::Info("ValidationService: Completed - %zu frames, dump: %s",
                  frameData.size(), path.c_str());
    } else {
        Log::Error("ValidationService: Failed to dump to %s", path.c_str());
    }

    if (m_EventBus) {
        m_EventBus->Publish(ValidationStoppedEvent{path, ok});
    }

    m_IsActive = false;
    m_OutputPath.clear();
    return ok ? Result<void>::Ok()
              : Result<void>::Error("Failed to write validation dump", "io");
}

void ValidationService::StopImmediate() {
    if (m_Recorder && m_Recorder->IsRecording()) {
        m_Recorder->Stop();
    }
    if (m_EventBus && m_IsActive) {
        m_EventBus->Publish(ValidationStoppedEvent{m_OutputPath, false});
    }
    m_IsActive = false;
    m_OutputPath.clear();
}
