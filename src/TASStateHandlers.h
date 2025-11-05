/**
 * @file TASStateHandlers.h
 * @brief State handler implementations for TASStateMachine
 *
 * This file contains concrete implementations of IStateHandler for each
 * state in the TAS system (Idle, Recording, Playing, etc.).
 */

#pragma once

#include "TASStateMachine.h"
#include "Result.h"
#include <memory>

// Forward declarations
class TASEngine;
class Recorder;
class RecordPlayer;
class ScriptContextManager;
class InputSystem;
class GameInterface;

/**
 * @brief Base handler with common functionality for all state handlers
 */
class BaseTASStateHandler : public TASStateMachine::IStateHandler {
public:
    explicit BaseTASStateHandler(TASEngine *engine);
    virtual ~BaseTASStateHandler() = default;

protected:
    TASEngine *m_Engine;

    // Helper methods for accessing TASEngine subsystems
    Recorder *GetRecorder() const;
    RecordPlayer *GetRecordPlayer() const;
    ScriptContextManager *GetScriptContextManager() const;
    InputSystem *GetInputSystem() const;
    GameInterface *GetGameInterface() const;
};

// ============================================================================
// IdleHandler - Handles idle/inactive state
// ============================================================================

/**
 * @brief Handler for idle state (no TAS activity)
 *
 * In this state, the TAS system is initialized but not actively
 * recording or playing back any content.
 */
class IdleHandler : public BaseTASStateHandler {
public:
    explicit IdleHandler(TASEngine *engine);

    Result<void> OnEnter() override;
    Result<void> OnExit() override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "IdleHandler"; }
};

// ============================================================================
// RecordingHandler - Handles recording state
// ============================================================================

/**
 * @brief Handler for recording state
 *
 * Records player input and game state for later playback or script generation.
 */
class RecordingHandler : public BaseTASStateHandler {
public:
    explicit RecordingHandler(TASEngine *engine);

    Result<void> OnEnter() override;
    Result<void> OnExit() override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "RecordingHandler"; }
};

// ============================================================================
// PlayingScriptHandler - Handles Lua script playback
// ============================================================================

/**
 * @brief Handler for Lua script playback state
 *
 * Executes Lua scripts that control game input programmatically.
 */
class PlayingScriptHandler : public BaseTASStateHandler {
public:
    explicit PlayingScriptHandler(TASEngine *engine);

    Result<void> OnEnter() override;
    Result<void> OnExit() override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "PlayingScriptHandler"; }
};

// ============================================================================
// PlayingRecordHandler - Handles binary record playback
// ============================================================================

/**
 * @brief Handler for binary record playback state
 *
 * Plays back pre-recorded input sequences from binary files.
 */
class PlayingRecordHandler : public BaseTASStateHandler {
public:
    explicit PlayingRecordHandler(TASEngine *engine);

    Result<void> OnEnter() override;
    Result<void> OnExit() override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "PlayingRecordHandler"; }
};

// ============================================================================
// TranslatingHandler - Handles translation (record to script conversion)
// ============================================================================

/**
 * @brief Handler for translation state
 *
 * Simultaneously plays back a record while recording it to generate
 * an equivalent Lua script (legacy record -> modern script conversion).
 */
class TranslatingHandler : public BaseTASStateHandler {
public:
    explicit TranslatingHandler(TASEngine *engine);

    Result<void> OnEnter() override;
    Result<void> OnExit() override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "TranslatingHandler"; }
};

// ============================================================================
// PausedHandler - Handles paused state (optional)
// ============================================================================

/**
 * @brief Handler for paused state
 *
 * Temporarily suspends playback while maintaining state for resumption.
 */
class PausedHandler : public BaseTASStateHandler {
public:
    explicit PausedHandler(TASEngine *engine);

    Result<void> OnEnter() override;
    Result<void> OnExit() override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "PausedHandler"; }
};
