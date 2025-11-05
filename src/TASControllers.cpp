/**
 * @file TASControllers.cpp
 * @brief Implementation of Controller classes
 */

#include "TASControllers.h"
#include "TASEngine.h"
#include "Recorder.h"
#include "RecordPlayer.h"
#include "ScriptContextManager.h"
#include "ScriptGenerator.h"
#include "InputSystem.h"
#include "GameInterface.h"
#include "Logger.h"

// ============================================================================
// RecordingController Implementation
// ============================================================================

RecordingController::RecordingController(TASEngine* engine)
    : m_Engine(engine) {
    if (!m_Engine) {
        throw std::invalid_argument("TASEngine cannot be null");
    }
}

Result<void> RecordingController::Initialize() {
    if (m_IsInitialized) {
        return Result<void>::Ok();
    }

    // Create default strategy (StandardRecorder)
    m_Strategy = std::make_unique<StandardRecorder>(m_Engine);

    auto result = m_Strategy->Start();
    if (!result.IsOk()) {
        return result;
    }

    // Stop immediately after init (just to initialize subsystems)
    m_Strategy->Stop();

    m_IsInitialized = true;
    Log::Info("RecordingController initialized");

    return Result<void>::Ok();
}

Result<void> RecordingController::StartRecording(bool useValidation) {
    if (!m_IsInitialized) {
        return Result<void>::Error("RecordingController not initialized", "state");
    }

    if (IsRecording()) {
        return Result<void>::Error("Already recording", "state");
    }

    // Create appropriate strategy
    if (useValidation) {
        auto innerStrategy = std::make_unique<StandardRecorder>(m_Engine);
        m_Strategy = std::make_unique<ValidationRecorder>(std::move(innerStrategy));
    } else {
        m_Strategy = std::make_unique<StandardRecorder>(m_Engine);
    }

    // Setup input system
    SetupInputSystemForRecording();

    // Start the strategy
    auto result = m_Strategy->Start();
    if (!result.IsOk()) {
        CleanupAfterRecording();
        return result;
    }

    Log::Info("RecordingController: Started %s recording",
             useValidation ? "validation" : "standard");

    return Result<void>::Ok();
}

Result<std::vector<FrameData>> RecordingController::StopRecording(bool immediate) {
    if (!IsRecording()) {
        return Result<std::vector<FrameData>>::Error("Not recording", "state");
    }

    // Stop the strategy
    auto result = m_Strategy->Stop();

    // Cleanup
    CleanupAfterRecording();

    if (result.IsOk()) {
        Log::Info("RecordingController: Stopped recording, captured %zu frames",
                 result.Unwrap().size());
    }

    return result;
}

bool RecordingController::IsRecording() const {
    return m_Strategy && m_Strategy->IsRecording();
}

size_t RecordingController::GetFrameCount() const {
    return m_Strategy ? m_Strategy->GetFrameCount() : 0;
}

void RecordingController::SetRecordingOptions(const IRecordingStrategy::Options& options) {
    if (m_Strategy) {
        m_Strategy->SetOptions(options);
    }
}

void RecordingController::SetupInputSystemForRecording() {
    auto inputSystem = m_Engine->GetInputSystem();
    if (inputSystem) {
        inputSystem->Reset();
        inputSystem->SetEnabled(false);  // Disable during recording
    }
}

void RecordingController::CleanupAfterRecording() {
    auto inputSystem = m_Engine->GetInputSystem();
    if (inputSystem) {
        inputSystem->Reset();
        inputSystem->SetEnabled(false);
    }

    auto gameInterface = m_Engine->GetGameInterface();
    if (gameInterface) {
        gameInterface->SetUIMode(UIMode::Idle);
    }
}

// ============================================================================
// PlaybackController Implementation
// ============================================================================

PlaybackController::PlaybackController(TASEngine* engine)
    : m_Engine(engine), m_CurrentType(PlaybackType::None) {
    if (!m_Engine) {
        throw std::invalid_argument("TASEngine cannot be null");
    }
}

Result<void> PlaybackController::Initialize() {
    if (m_IsInitialized) {
        return Result<void>::Ok();
    }

    m_IsInitialized = true;
    Log::Info("PlaybackController initialized");

    return Result<void>::Ok();
}

Result<void> PlaybackController::StartPlayback(TASProject* project, PlaybackType type) {
    if (!m_IsInitialized) {
        return Result<void>::Error("PlaybackController not initialized", "state");
    }

    if (!project) {
        return Result<void>::Error("Project cannot be null", "invalid_argument");
    }

    if (IsPlaying()) {
        return Result<void>::Error("Already playing", "state");
    }

    // Create appropriate strategy
    auto strategyResult = CreateStrategy(type);
    if (!strategyResult.IsOk()) {
        return Result<void>::Error(strategyResult.GetError());
    }

    m_Strategy = std::move(strategyResult.Unwrap());
    m_CurrentType = type;
    m_CurrentProject = project;

    // Setup input system
    SetupInputSystemForPlayback(type);

    // Start playback
    auto result = m_Strategy->LoadAndPlay(project);
    if (!result.IsOk()) {
        CleanupAfterPlayback();
        m_Strategy.reset();
        m_CurrentType = PlaybackType::None;
        m_CurrentProject = nullptr;
        return result;
    }

    Log::Info("PlaybackController: Started %s playback for project '%s'",
             type == PlaybackType::Script ? "script" : "record",
             project->GetName().c_str());

    return Result<void>::Ok();
}

void PlaybackController::StopPlayback(bool clearProject) {
    if (!IsPlaying()) {
        return;
    }

    if (m_Strategy) {
        m_Strategy->Stop();
    }

    CleanupAfterPlayback();

    if (clearProject) {
        m_CurrentProject = nullptr;
    }

    m_Strategy.reset();
    m_CurrentType = PlaybackType::None;

    Log::Info("PlaybackController: Stopped playback");
}

void PlaybackController::Pause() {
    if (m_Strategy && IsPlaying()) {
        m_Strategy->Pause();
        Log::Info("PlaybackController: Paused");
    }
}

void PlaybackController::Resume() {
    if (m_Strategy && IsPaused()) {
        m_Strategy->Resume();
        Log::Info("PlaybackController: Resumed");
    }
}

bool PlaybackController::IsPlaying() const {
    return m_Strategy && m_Strategy->IsPlaying();
}

bool PlaybackController::IsPaused() const {
    return m_Strategy && m_Strategy->IsPaused();
}

float PlaybackController::GetProgress() const {
    return m_Strategy ? m_Strategy->GetProgress() : 0.0f;
}

void PlaybackController::SetupInputSystemForPlayback(PlaybackType type) {
    auto inputSystem = m_Engine->GetInputSystem();
    if (!inputSystem) {
        return;
    }

    if (type == PlaybackType::Script) {
        // For script playback, enable InputSystem for deterministic replay
        inputSystem->SetEnabled(true);
        inputSystem->Reset();
    } else if (type == PlaybackType::Record) {
        // For record playback, DISABLE InputSystem
        // Record playback applies input directly to keyboard state buffer
        inputSystem->SetEnabled(false);
        inputSystem->Reset();
    }
}

void PlaybackController::CleanupAfterPlayback() {
    auto inputSystem = m_Engine->GetInputSystem();
    if (inputSystem) {
        inputSystem->Reset();
        inputSystem->SetEnabled(false);
    }

    auto gameInterface = m_Engine->GetGameInterface();
    if (gameInterface) {
        gameInterface->SetUIMode(UIMode::Idle);
    }
}

Result<std::unique_ptr<IPlaybackStrategy>> PlaybackController::CreateStrategy(PlaybackType type) {
    if (type == PlaybackType::Script) {
        auto strategy = std::make_unique<ScriptPlaybackStrategy>(m_Engine);
        auto result = strategy->Initialize();
        if (!result.IsOk()) {
            return Result<std::unique_ptr<IPlaybackStrategy>>::Error(result.GetError());
        }
        return Result<std::unique_ptr<IPlaybackStrategy>>::Ok(std::move(strategy));
    } else if (type == PlaybackType::Record) {
        auto strategy = std::make_unique<RecordPlaybackStrategy>(m_Engine);
        auto result = strategy->Initialize();
        if (!result.IsOk()) {
            return Result<std::unique_ptr<IPlaybackStrategy>>::Error(result.GetError());
        }
        return Result<std::unique_ptr<IPlaybackStrategy>>::Ok(std::move(strategy));
    } else {
        return Result<std::unique_ptr<IPlaybackStrategy>>::Error(
            "Invalid playback type", "invalid_argument");
    }
}

// ============================================================================
// TranslationController Implementation
// ============================================================================

TranslationController::TranslationController(TASEngine* engine)
    : m_Engine(engine) {
    if (!m_Engine) {
        throw std::invalid_argument("TASEngine cannot be null");
    }
}

Result<void> TranslationController::Initialize() {
    if (m_IsInitialized) {
        return Result<void>::Ok();
    }

    m_IsInitialized = true;
    Log::Info("TranslationController initialized");

    return Result<void>::Ok();
}

Result<void> TranslationController::StartTranslation(TASProject* project,
                                                      const GenerationOptions& options) {
    if (!m_IsInitialized) {
        return Result<void>::Error("TranslationController not initialized", "state");
    }

    if (!project) {
        return Result<void>::Error("Project cannot be null", "invalid_argument");
    }

    if (m_IsTranslating) {
        return Result<void>::Error("Already translating", "state");
    }

    auto recorder = m_Engine->GetRecorder();
    auto recordPlayer = m_Engine->GetRecordPlayer();

    if (!recorder || !recordPlayer) {
        return Result<void>::Error("Recorder or RecordPlayer not available", "subsystem");
    }

    // Store generation options
    m_GenerationOptions = options;

    // Configure recorder for translation
    recorder->SetGenerationOptions(options);
    recorder->SetUpdateRate(project->GetUpdateRate());
    recorder->SetAutoGenerate(true);
    recorder->SetTranslationMode(true);

    // Start recorder
    recorder->Start();
    if (!recorder->IsRecording()) {
        return Result<void>::Error("Failed to start recorder", "recording");
    }

    // Start record playback
    bool success = recordPlayer->LoadAndPlay(project);
    if (!success) {
        recorder->Stop();  // Clean up
        return Result<void>::Error("Failed to start record playback for translation", "playback");
    }

    m_IsTranslating = true;
    m_CurrentProject = project;

    Log::Info("TranslationController: Started translation for project '%s'",
             project->GetName().c_str());

    return Result<void>::Ok();
}

void TranslationController::StopTranslation(bool clearProject) {
    if (!m_IsTranslating) {
        return;
    }

    // Stop record playback
    auto recordPlayer = m_Engine->GetRecordPlayer();
    if (recordPlayer) {
        recordPlayer->Stop();
    }

    // Stop recorder (will auto-generate script if configured)
    auto recorder = m_Engine->GetRecorder();
    if (recorder && recorder->IsRecording()) {
        recorder->Stop();
        Log::Info("TranslationController: Recorder stopped, script generated");
    }

    m_IsTranslating = false;

    if (clearProject) {
        m_CurrentProject = nullptr;
    }

    auto gameInterface = m_Engine->GetGameInterface();
    if (gameInterface) {
        gameInterface->SetUIMode(UIMode::Idle);
    }

    Log::Info("TranslationController: Stopped translation");
}

bool TranslationController::IsTranslating() const {
    return m_IsTranslating;
}

float TranslationController::GetProgress() const {
    if (!m_IsTranslating) {
        return 0.0f;
    }

    auto recordPlayer = m_Engine->GetRecordPlayer();
    if (!recordPlayer) {
        return 0.0f;
    }

    size_t current = recordPlayer->GetCurrentFrame();
    size_t total = recordPlayer->GetTotalFrames();

    return total > 0 ? static_cast<float>(current) / total : 0.0f;
}

void TranslationController::OnTranslationPlaybackComplete() {
    if (!m_IsTranslating) {
        return;
    }

    // Stop translation (Recorder will auto-generate script)
    StopTranslation(false);
}

void TranslationController::GenerateScript() {
    // This method is kept for interface compatibility but is not used.
    // Script generation is now handled automatically by Recorder when it stops
    // in auto-generate mode.
    Log::Warn("TranslationController::GenerateScript() called, but script generation is automatic");
}
