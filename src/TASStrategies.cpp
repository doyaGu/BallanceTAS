/**
 * @file TASStrategies.cpp
 * @brief Implementation of Strategy Pattern for Recording and Playback
 *
 * This file implements concrete strategies that wrap existing subsystems
 * (Recorder, RecordPlayer, ScriptContextManager) to provide a uniform
 * Strategy Pattern interface for TASEngine.
 */

#include "TASStrategies.h"
#include "TASEngine.h"
#include "Recorder.h"
#include "RecordPlayer.h"
#include "ScriptContextManager.h"
#include "ScriptContext.h"
#include "Logger.h"

// ============================================================================
// ScriptPlaybackStrategy Implementation (Wrapper for ScriptContextManager)
// ============================================================================

ScriptPlaybackStrategy::ScriptPlaybackStrategy(TASEngine *engine) : m_Engine(engine) {
    if (!m_Engine) {
        throw std::invalid_argument("TASEngine cannot be null");
    }
}

Result<void> ScriptPlaybackStrategy::Initialize() {
    // ScriptContextManager is initialized by TASEngine
    // This strategy just wraps its functionality
    return Result<void>::Ok();
}

Result<void> ScriptPlaybackStrategy::LoadAndPlay(TASProject *project) {
    if (!project) {
        return Result<void>::Error("Project cannot be null", "invalid_argument");
    }

    auto scriptManager = m_Engine->GetScriptContextManager();
    if (!scriptManager) {
        return Result<void>::Error("ScriptContextManager not available", "subsystem");
    }

    // Determine context type based on project
    bool isGlobal = project->IsGlobalProject();
    std::string contextName = isGlobal ? "global" : "level_" + project->GetName();

    // Get or create appropriate context
    auto ctx = isGlobal
                   ? scriptManager->GetOrCreateGlobalContext()
                   : scriptManager->GetOrCreateLevelContext(project->GetName());

    if (!ctx) {
        return Result<void>::Error("Failed to create script context", "context");
    }

    // Load and execute the project
    bool success = ctx->LoadAndExecute(project);
    if (!success) {
        return Result<void>::Error("Failed to load and execute script project", "execution");
    }

    m_CurrentProject = project;
    m_IsPlaying = true;
    m_IsPaused = false;

    NotifyStatusChanged();

    Log::Info("ScriptPlaybackStrategy: Started playing project '%s' in context '%s'",
              project->GetName().c_str(), contextName.c_str());

    return Result<void>::Ok();
}

void ScriptPlaybackStrategy::Tick() {
    // Script execution is handled by ScriptContext::Tick() via LuaScheduler
    // This is called automatically by TASEngine's callback system
    // No additional work needed here
}

void ScriptPlaybackStrategy::Stop() {
    if (!m_IsPlaying) {
        return;
    }

    auto scriptManager = m_Engine->GetScriptContextManager();
    if (scriptManager) {
        // Stop all active contexts
        auto contexts = scriptManager->GetContextsByPriority();
        for (const auto &ctx : contexts) {
            if (ctx && ctx->IsExecuting()) {
                ctx->Stop();
            }
        }
    }

    m_IsPlaying = false;
    m_IsPaused = false;
    m_CurrentProject = nullptr;

    NotifyStatusChanged();

    Log::Info("ScriptPlaybackStrategy: Stopped playback");
}

void ScriptPlaybackStrategy::Pause() {
    if (!m_IsPlaying || m_IsPaused) {
        return;
    }

    m_IsPaused = true;
    // TODO: Implement pause for script execution (requires ScriptContext changes)
    Log::Info("ScriptPlaybackStrategy: Paused (note: full pause not yet implemented)");
}

void ScriptPlaybackStrategy::Resume() {
    if (!m_IsPlaying || !m_IsPaused) {
        return;
    }

    m_IsPaused = false;
    // TODO: Implement resume for script execution
    Log::Info("ScriptPlaybackStrategy: Resumed");
}

size_t ScriptPlaybackStrategy::GetCurrentTick() const {
    return m_Engine ? m_Engine->GetCurrentTick() : 0;
}

void ScriptPlaybackStrategy::NotifyStatusChanged() {
    if (m_StatusCallback) {
        m_StatusCallback(m_IsPlaying);
    }
}

// ============================================================================
// RecordPlaybackStrategy Implementation (Wrapper for RecordPlayer)
// ============================================================================

RecordPlaybackStrategy::RecordPlaybackStrategy(TASEngine *engine)
    : m_Engine(engine) {
    if (!m_Engine) {
        throw std::invalid_argument("TASEngine cannot be null");
    }
}

Result<void> RecordPlaybackStrategy::Initialize() {
    // RecordPlayer is initialized by TASEngine
    return Result<void>::Ok();
}

Result<void> RecordPlaybackStrategy::LoadAndPlay(TASProject *project) {
    if (!project) {
        return Result<void>::Error("Project cannot be null", "invalid_argument");
    }

    auto recordPlayer = m_Engine->GetRecordPlayer();
    if (!recordPlayer) {
        return Result<void>::Error("RecordPlayer not available", "subsystem");
    }

    // Load and start playback
    bool success = recordPlayer->LoadAndPlay(project);
    if (!success) {
        return Result<void>::Error("Failed to load and play record", "playback");
    }

    // Get frame count from RecordPlayer
    m_TotalFrames = recordPlayer->GetTotalFrames();
    m_CurrentFrameIndex = 0;
    m_IsPlaying = true;
    m_IsPaused = false;

    NotifyStatusChanged();
    NotifyProgress();

    Log::Info("RecordPlaybackStrategy: Started playing record with %zu frames",
              m_TotalFrames);

    return Result<void>::Ok();
}

void RecordPlaybackStrategy::Tick() {
    if (!m_IsPlaying || m_IsPaused) {
        return;
    }

    auto recordPlayer = m_Engine->GetRecordPlayer();
    if (recordPlayer) {
        m_CurrentFrameIndex = recordPlayer->GetCurrentFrame();
        NotifyProgress();
    }
}

void RecordPlaybackStrategy::Stop() {
    if (!m_IsPlaying) {
        return;
    }

    auto recordPlayer = m_Engine->GetRecordPlayer();
    if (recordPlayer) {
        recordPlayer->Stop();
    }

    m_IsPlaying = false;
    m_IsPaused = false;
    m_CurrentFrameIndex = 0;
    m_TotalFrames = 0;

    NotifyStatusChanged();

    Log::Info("RecordPlaybackStrategy: Stopped playback");
}

void RecordPlaybackStrategy::Pause() {
    if (!m_IsPlaying || m_IsPaused) {
        return;
    }

    auto recordPlayer = m_Engine->GetRecordPlayer();
    if (recordPlayer) {
        recordPlayer->Pause();
    }

    m_IsPaused = true;
    Log::Info("RecordPlaybackStrategy: Paused at frame %zu/%zu",
              m_CurrentFrameIndex, m_TotalFrames);
}

void RecordPlaybackStrategy::Resume() {
    if (!m_IsPlaying || !m_IsPaused) {
        return;
    }

    auto recordPlayer = m_Engine->GetRecordPlayer();
    if (recordPlayer) {
        recordPlayer->Resume();
    }

    m_IsPaused = false;
    Log::Info("RecordPlaybackStrategy: Resumed from frame %zu/%zu",
              m_CurrentFrameIndex, m_TotalFrames);
}

void RecordPlaybackStrategy::NotifyStatusChanged() {
    if (m_StatusCallback) {
        m_StatusCallback(m_IsPlaying);
    }
}

void RecordPlaybackStrategy::NotifyProgress() {
    if (m_ProgressCallback) {
        m_ProgressCallback(m_CurrentFrameIndex, m_TotalFrames);
    }
}

// ============================================================================
// StandardRecorder Implementation (Wrapper for Recorder)
// ============================================================================

StandardRecorder::StandardRecorder(TASEngine *engine)
    : m_Engine(engine) {
    if (!m_Engine) {
        throw std::invalid_argument("TASEngine cannot be null");
    }
}

Result<void> StandardRecorder::Start() {
    if (m_IsRecording) {
        return Result<void>::Error("Already recording", "state");
    }

    auto recorder = m_Engine->GetRecorder();
    if (!recorder) {
        return Result<void>::Error("Recorder not available", "subsystem");
    }

    // Start recording via the Recorder subsystem
    recorder->Start();

    m_IsRecording = true;
    m_Frames.clear();
    m_PreviousKeyState.clear();
    m_PreviousKeyState.resize(256, 0);

    Log::Info("StandardRecorder: Started recording");

    return Result<void>::Ok();
}

void StandardRecorder::Tick(size_t currentTick, const unsigned char *keyboardState) {
    if (!m_IsRecording) {
        return;
    }

    auto recorder = m_Engine->GetRecorder();
    if (recorder) {
        recorder->Tick(currentTick, keyboardState);
    }

    // Update previous key state
    std::copy(keyboardState, keyboardState + 256, m_PreviousKeyState.begin());
}

Result<std::vector<FrameData>> StandardRecorder::Stop() {
    if (!m_IsRecording) {
        return Result<std::vector<FrameData>>::Error("Not recording", "state");
    }

    auto recorder = m_Engine->GetRecorder();
    if (!recorder) {
        return Result<std::vector<FrameData>>::Error("Recorder not available", "subsystem");
    }

    // Stop recording and get the data
    std::vector<FrameData> frames = recorder->Stop();

    m_IsRecording = false;
    m_Frames = frames;

    Log::Info("StandardRecorder: Stopped recording, captured %zu frames", frames.size());

    return Result<std::vector<FrameData>>::Ok(std::move(frames));
}

bool StandardRecorder::HasKeyStateChanged(const unsigned char *currentState) const {
    if (m_PreviousKeyState.empty()) {
        return true; // First frame, always changed
    }

    for (size_t i = 0; i < 256; ++i) {
        if (currentState[i] != m_PreviousKeyState[i]) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// ValidationRecorder Implementation (Decorator Pattern)
// ============================================================================

ValidationRecorder::ValidationRecorder(std::unique_ptr<IRecordingStrategy> innerRecorder)
    : m_InnerRecorder(std::move(innerRecorder)) {
    if (!m_InnerRecorder) {
        throw std::invalid_argument("Inner recorder cannot be null");
    }
}

Result<void> ValidationRecorder::Start() {
    m_ValidationData.clear();
    return m_InnerRecorder->Start();
}

void ValidationRecorder::Tick(size_t currentTick, const unsigned char *keyboardState) {
    // First, let the inner recorder do its work
    m_InnerRecorder->Tick(currentTick, keyboardState);

    // Then capture additional validation data
    CaptureValidationData(currentTick);
}

Result<std::vector<FrameData>> ValidationRecorder::Stop() {
    auto result = m_InnerRecorder->Stop();

    if (result.IsOk()) {
        Log::Info("ValidationRecorder: Captured %zu frames with %zu validation data points",
                  result.Unwrap().size(), m_ValidationData.size());
    }

    return result;
}

void ValidationRecorder::SetOptions(const Options &options) {
    m_InnerRecorder->SetOptions(options);

    // Force enable game state capture for validation
    auto modifiedOptions = options;
    modifiedOptions.captureGameState = true;
    m_InnerRecorder->SetOptions(modifiedOptions);
}

void ValidationRecorder::CaptureValidationData(size_t currentTick) {
    // TODO: Implement actual validation data capture from GameInterface
    // For now, just record a placeholder
    ValidationData data;
    data.ballPosition[0] = 0.0f;
    data.ballPosition[1] = 0.0f;
    data.ballPosition[2] = 0.0f;
    data.ballVelocity[0] = 0.0f;
    data.ballVelocity[1] = 0.0f;
    data.ballVelocity[2] = 0.0f;
    data.currentLevel = 0;
    data.timestamp = currentTick;

    m_ValidationData.push_back(data);
}
