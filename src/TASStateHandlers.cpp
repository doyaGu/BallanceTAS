/**
 * @file TASStateHandlers.cpp
 * @brief Implementation of state handlers for TASStateMachine
 */

#include "TASStateHandlers.h"
#include "TASEngine.h"
#include "Recorder.h"
#include "RecordPlayer.h"
#include "ScriptContextManager.h"
#include "InputSystem.h"
#include "GameInterface.h"
#include "Logger.h"

// ============================================================================
// BaseTASStateHandler Implementation
// ============================================================================

BaseTASStateHandler::BaseTASStateHandler(TASEngine *engine)
    : m_Engine(engine) {
    if (!m_Engine) {
        throw std::invalid_argument("TASEngine cannot be null");
    }
}

Recorder *BaseTASStateHandler::GetRecorder() const {
    return m_Engine ? m_Engine->GetRecorder() : nullptr;
}

RecordPlayer *BaseTASStateHandler::GetRecordPlayer() const {
    return m_Engine ? m_Engine->GetRecordPlayer() : nullptr;
}

ScriptContextManager *BaseTASStateHandler::GetScriptContextManager() const {
    return m_Engine ? m_Engine->GetScriptContextManager() : nullptr;
}

InputSystem *BaseTASStateHandler::GetInputSystem() const {
    return m_Engine ? m_Engine->GetInputSystem() : nullptr;
}

GameInterface *BaseTASStateHandler::GetGameInterface() const {
    return m_Engine ? m_Engine->GetGameInterface() : nullptr;
}

// ============================================================================
// IdleHandler Implementation
// ============================================================================

IdleHandler::IdleHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> IdleHandler::OnEnter() {
    Log::Info("Entering Idle state");

    // Ensure all subsystems are stopped and cleaned up
    auto inputSystem = GetInputSystem();
    if (inputSystem) {
        inputSystem->Reset();
        inputSystem->SetEnabled(false);
    }

    // Set UI to idle mode
    auto gameInterface = GetGameInterface();
    if (gameInterface) {
        gameInterface->SetUIMode(UIMode::Idle);
    }

    return Result<void>::Ok();
}

Result<void> IdleHandler::OnExit() {
    Log::Info("Exiting Idle state");
    return Result<void>::Ok();
}

void IdleHandler::OnTick() {
    // Idle state does nothing on tick
}

bool IdleHandler::CanTransitionTo(TASStateMachine::State newState) const {
    // From Idle, can transition to any active state
    return newState == TASStateMachine::State::Recording ||
        newState == TASStateMachine::State::PlayingScript ||
        newState == TASStateMachine::State::PlayingRecord ||
        newState == TASStateMachine::State::Translating;
}

// ============================================================================
// RecordingHandler Implementation
// ============================================================================

RecordingHandler::RecordingHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> RecordingHandler::OnEnter() {
    Log::Info("Entering Recording state");

    // Ensure InputSystem is DISABLED during recording
    // We want to capture the user's actual input, not override it
    auto inputSystem = GetInputSystem();
    if (inputSystem) {
        inputSystem->Reset();
        inputSystem->SetEnabled(false);
    }

    // Start the recorder
    auto recorder = GetRecorder();
    if (!recorder) {
        return Result<void>::Error("Recorder subsystem not available", "subsystem");
    }

    if (recorder->IsRecording()) {
        return Result<void>::Error("Recorder already active", "state");
    }

    try {
        recorder->Start();
    } catch (const std::exception &e) {
        return Result<void>::Error(
            std::string("Failed to start recorder: ") + e.what(),
            "recorder"
        );
    }

    // Set UI to recording mode
    auto gameInterface = GetGameInterface();
    if (gameInterface) {
        gameInterface->SetUIMode(UIMode::Recording);
    }

    return Result<void>::Ok();
}

Result<void> RecordingHandler::OnExit() {
    Log::Info("Exiting Recording state");

    // Stop the recorder
    auto recorder = GetRecorder();
    if (recorder && recorder->IsRecording()) {
        try {
            recorder->Stop();
        } catch (const std::exception &e) {
            Log::Error("Exception while stopping recorder: %s", e.what());
            // Continue with cleanup even if stop fails
        }
    }

    // Ensure InputSystem remains disabled after recording
    auto inputSystem = GetInputSystem();
    if (inputSystem) {
        inputSystem->Reset();
        inputSystem->SetEnabled(false);
    }

    return Result<void>::Ok();
}

void RecordingHandler::OnTick() {
    // Recording tick is handled by callbacks
    // (registered in TASEngine::SetupRecordingCallbacks)
}

bool RecordingHandler::CanTransitionTo(TASStateMachine::State newState) const {
    // From Recording, can only transition to Idle or Paused
    return newState == TASStateMachine::State::Idle ||
        newState == TASStateMachine::State::Paused;
}

// ============================================================================
// PlayingScriptHandler Implementation
// ============================================================================

PlayingScriptHandler::PlayingScriptHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> PlayingScriptHandler::OnEnter() {
    Log::Info("Entering PlayingScript state");

    // Enable InputSystem for deterministic replay
    auto inputSystem = GetInputSystem();
    if (inputSystem) {
        inputSystem->SetEnabled(true);
        inputSystem->Reset(); // Start with clean state
    }

    // Note: Script loading and execution is handled by TASEngine::StartReplayInternal
    // This handler focuses on state management, not execution setup

    // Set UI to playing mode
    auto gameInterface = GetGameInterface();
    if (gameInterface) {
        gameInterface->SetUIMode(UIMode::Playing);
    }

    return Result<void>::Ok();
}

Result<void> PlayingScriptHandler::OnExit() {
    Log::Info("Exiting PlayingScript state");

    // Stop all active script contexts
    auto scriptManager = GetScriptContextManager();
    if (scriptManager) {
        auto contexts = scriptManager->GetContextsByPriority();
        for (const auto &ctx : contexts) {
            if (ctx && ctx->IsExecuting()) {
                Log::Info("Stopping script execution in context: %s", ctx->GetName().c_str());
                ctx->Stop();
            }
        }
    }

    // Clean up input state
    auto inputSystem = GetInputSystem();
    if (inputSystem) {
        inputSystem->Reset();
        inputSystem->SetEnabled(false);
    }

    return Result<void>::Ok();
}

void PlayingScriptHandler::OnTick() {
    // Script playback tick is handled by callbacks
    // (registered in TASEngine::SetupScriptPlaybackCallbacks)
}

bool PlayingScriptHandler::CanTransitionTo(TASStateMachine::State newState) const {
    // From PlayingScript, can transition to Idle or Paused
    return newState == TASStateMachine::State::Idle ||
        newState == TASStateMachine::State::Paused;
}

// ============================================================================
// PlayingRecordHandler Implementation
// ============================================================================

PlayingRecordHandler::PlayingRecordHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> PlayingRecordHandler::OnEnter() {
    Log::Info("Entering PlayingRecord state");

    // For record playback, DISABLE InputSystem completely
    // Record playback applies input directly to keyboard state buffer
    auto inputSystem = GetInputSystem();
    if (inputSystem) {
        inputSystem->SetEnabled(false);
        inputSystem->Reset(); // Ensure clean state
    }

    // Note: Record loading and playback is handled by TASEngine::StartReplayInternal
    // This handler focuses on state management

    // Set UI to playing mode
    auto gameInterface = GetGameInterface();
    if (gameInterface) {
        gameInterface->SetUIMode(UIMode::Playing);
    }

    return Result<void>::Ok();
}

Result<void> PlayingRecordHandler::OnExit() {
    Log::Info("Exiting PlayingRecord state");

    // Stop the record player
    auto recordPlayer = GetRecordPlayer();
    if (recordPlayer && recordPlayer->IsPlaying()) {
        try {
            recordPlayer->Stop();
        } catch (const std::exception &e) {
            Log::Error("Exception while stopping record player: %s", e.what());
        }
    }

    // Clean up input state
    auto inputSystem = GetInputSystem();
    if (inputSystem) {
        inputSystem->Reset();
        inputSystem->SetEnabled(false);
    }

    return Result<void>::Ok();
}

void PlayingRecordHandler::OnTick() {
    // Record playback tick is handled by callbacks
    // (registered in TASEngine::SetupRecordPlaybackCallbacks)
}

bool PlayingRecordHandler::CanTransitionTo(TASStateMachine::State newState) const {
    // From PlayingRecord, can transition to Idle or Paused
    return newState == TASStateMachine::State::Idle ||
        newState == TASStateMachine::State::Paused;
}

// ============================================================================
// TranslatingHandler Implementation
// ============================================================================

TranslatingHandler::TranslatingHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> TranslatingHandler::OnEnter() {
    Log::Info("Entering Translating state");

    // For translation, InputSystem should be DISABLED
    // We want RecordPlayer to control input directly, and Recorder to capture it
    auto inputSystem = GetInputSystem();
    if (inputSystem) {
        inputSystem->Reset();
        inputSystem->SetEnabled(false);
    }

    // Note: Translation setup (starting both RecordPlayer and Recorder)
    // is handled by TASEngine::StartTranslationInternal

    // Set UI to recording mode (since we're generating a script)
    auto gameInterface = GetGameInterface();
    if (gameInterface) {
        gameInterface->SetUIMode(UIMode::Recording);
    }

    return Result<void>::Ok();
}

Result<void> TranslatingHandler::OnExit() {
    Log::Info("Exiting Translating state");

    // Stop both record playback and recording
    auto recordPlayer = GetRecordPlayer();
    if (recordPlayer && recordPlayer->IsPlaying()) {
        try {
            recordPlayer->Stop();
        } catch (const std::exception &e) {
            Log::Error("Exception while stopping record player: %s", e.what());
        }
    }

    auto recorder = GetRecorder();
    if (recorder && recorder->IsRecording()) {
        try {
            recorder->Stop();
        } catch (const std::exception &e) {
            Log::Error("Exception while stopping recorder: %s", e.what());
        }
    }

    // Clean up input state
    auto inputSystem = GetInputSystem();
    if (inputSystem) {
        inputSystem->Reset();
        inputSystem->SetEnabled(false);
    }

    return Result<void>::Ok();
}

void TranslatingHandler::OnTick() {
    // Translation tick is handled by callbacks
    // (registered in TASEngine::SetupTranslationCallbacks)
}

bool TranslatingHandler::CanTransitionTo(TASStateMachine::State newState) const {
    // From Translating, can only transition to Idle
    // (Translation doesn't support pausing)
    return newState == TASStateMachine::State::Idle;
}

// ============================================================================
// PausedHandler Implementation
// ============================================================================

PausedHandler::PausedHandler(TASEngine *engine)
    : BaseTASStateHandler(engine) {
}

Result<void> PausedHandler::OnEnter() {
    Log::Info("Entering Paused state");

    // Pausing is primarily a state indicator
    // The actual pausing logic (stopping callbacks, etc.) is handled elsewhere
    // This handler just manages the state transition

    // Keep UI in current mode (don't change to idle)
    // The UI will show "Paused" overlay

    return Result<void>::Ok();
}

Result<void> PausedHandler::OnExit() {
    Log::Info("Exiting Paused state");

    // Resuming from pause
    // The previous state's handler will be re-entered

    return Result<void>::Ok();
}

void PausedHandler::OnTick() {
    // Paused state does nothing on tick
}

bool PausedHandler::CanTransitionTo(TASStateMachine::State newState) const {
    // From Paused, can resume to playing states or go to Idle
    return newState == TASStateMachine::State::Idle ||
        newState == TASStateMachine::State::PlayingScript ||
        newState == TASStateMachine::State::PlayingRecord;
}
