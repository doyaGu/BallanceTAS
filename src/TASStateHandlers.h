#pragma once

#include "Result.h"
#include "ServiceContainer.h"
#include "TASStateMachine.h"

class TASEngine;
class InputSystem;
class GameInterface;
class ProjectManager;
class Recorder;
class RecordPlayer;
class ScriptContextManager;
class RecordingService;
class PlaybackService;
class TranslationService;
class ValidationService;
enum class UIMode;

class BaseTASStateHandler : public TASStateMachine::IStateHandler {
public:
    explicit BaseTASStateHandler(TASEngine *engine);
    ~BaseTASStateHandler() override = default;

protected:
    TASEngine *m_Engine;
    ServiceProvider &Services() const;

    Recorder *GetRecorder() const;
    RecordPlayer *GetRecordPlayer() const;
    ScriptContextManager *GetScriptContextManager() const;
    InputSystem *GetInputSystem() const;
    GameInterface *GetGameInterface() const;
    ProjectManager *GetProjectManager() const;
    RecordingService *GetRecordingService() const;
    PlaybackService *GetPlaybackService() const;
    TranslationService *GetTranslationService() const;
    ValidationService *GetValidationService() const;

    void SetUIMode(UIMode mode) const;
    void ResetInputSystem() const;
    void StopValidationImmediate() const;
    void StopValidationGraceful() const;
};

class IdleHandler final : public BaseTASStateHandler {
public:
    explicit IdleHandler(TASEngine *engine);

    Result<void> OnEnter(TASStateMachine::State previousState) override;
    Result<void> OnExit(TASStateMachine::State nextState) override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "IdleHandler"; }
};

class PendingRecordHandler final : public BaseTASStateHandler {
public:
    explicit PendingRecordHandler(TASEngine *engine);

    Result<void> OnEnter(TASStateMachine::State previousState) override;
    Result<void> OnExit(TASStateMachine::State nextState) override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "PendingRecordHandler"; }
};

class RecordingHandler final : public BaseTASStateHandler {
public:
    explicit RecordingHandler(TASEngine *engine);

    Result<void> OnEnter(TASStateMachine::State previousState) override;
    Result<void> OnExit(TASStateMachine::State nextState) override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "RecordingHandler"; }
};

class PendingScriptPlaybackHandler final : public BaseTASStateHandler {
public:
    explicit PendingScriptPlaybackHandler(TASEngine *engine);

    Result<void> OnEnter(TASStateMachine::State previousState) override;
    Result<void> OnExit(TASStateMachine::State nextState) override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "PendingScriptPlaybackHandler"; }
};

class PendingRecordPlaybackHandler final : public BaseTASStateHandler {
public:
    explicit PendingRecordPlaybackHandler(TASEngine *engine);

    Result<void> OnEnter(TASStateMachine::State previousState) override;
    Result<void> OnExit(TASStateMachine::State nextState) override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "PendingRecordPlaybackHandler"; }
};

class PlayingScriptHandler final : public BaseTASStateHandler {
public:
    explicit PlayingScriptHandler(TASEngine *engine);

    Result<void> OnEnter(TASStateMachine::State previousState) override;
    Result<void> OnExit(TASStateMachine::State nextState) override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "PlayingScriptHandler"; }
};

class PlayingRecordHandler final : public BaseTASStateHandler {
public:
    explicit PlayingRecordHandler(TASEngine *engine);

    Result<void> OnEnter(TASStateMachine::State previousState) override;
    Result<void> OnExit(TASStateMachine::State nextState) override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "PlayingRecordHandler"; }
};

class PendingTranslationHandler final : public BaseTASStateHandler {
public:
    explicit PendingTranslationHandler(TASEngine *engine);

    Result<void> OnEnter(TASStateMachine::State previousState) override;
    Result<void> OnExit(TASStateMachine::State nextState) override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "PendingTranslationHandler"; }
};

class TranslatingHandler final : public BaseTASStateHandler {
public:
    explicit TranslatingHandler(TASEngine *engine);

    Result<void> OnEnter(TASStateMachine::State previousState) override;
    Result<void> OnExit(TASStateMachine::State nextState) override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "TranslatingHandler"; }
};

class PausedHandler final : public BaseTASStateHandler {
public:
    explicit PausedHandler(TASEngine *engine);

    Result<void> OnEnter(TASStateMachine::State previousState) override;
    Result<void> OnExit(TASStateMachine::State nextState) override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "PausedHandler"; }
};

class ShuttingDownHandler final : public BaseTASStateHandler {
public:
    explicit ShuttingDownHandler(TASEngine *engine);

    Result<void> OnEnter(TASStateMachine::State previousState) override;
    Result<void> OnExit(TASStateMachine::State nextState) override;
    void OnTick() override;
    bool CanTransitionTo(TASStateMachine::State newState) const override;
    const char *GetStateName() const override { return "ShuttingDownHandler"; }
};
