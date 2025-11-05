/**
 * @file TASControllers.cpp
 * @brief Implementation of Controller classes
 */

#include "TASControllers.h"
#include "ServiceContainer.h"
#include "Recorder.h"
#include "RecordPlayer.h"
#include "ScriptGenerator.h"
#include "InputSystem.h"
#include "DX8InputManager.h"
#include "GameInterface.h"
#include "ScriptContextManager.h"
#include "ProjectManager.h"
#include "TASHook.h"
#include "Logger.h"

#ifdef ENABLE_REPL
#include "LuaREPLServer.h"
#endif

// ============================================================================
// RecordingController Implementation
// ============================================================================

RecordingController::RecordingController(ServiceProvider *provider) : m_ServiceProvider(provider) {
    if (!m_ServiceProvider) {
        throw std::invalid_argument("ServiceProvider cannot be null");
    }
}

Result<void> RecordingController::Initialize() {
    if (m_IsInitialized) {
        return Result<void>::Ok();
    }

    // Create default strategy (StandardRecorder)
    m_Strategy = std::make_unique<StandardRecorder>(m_ServiceProvider);

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
        auto innerStrategy = std::make_unique<StandardRecorder>(m_ServiceProvider);
        m_Strategy = std::make_unique<ValidationRecorder>(std::move(innerStrategy));
    } else {
        m_Strategy = std::make_unique<StandardRecorder>(m_ServiceProvider);
    }

    // Reset tick counter
    ResetTick();

    // Setup input system
    SetupInputSystemForRecording();

    // Set up callbacks for recording
    SetupCallbacks();

    // Start the strategy
    auto result = m_Strategy->Start();
    if (!result.IsOk()) {
        ClearCallbacks();
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

    // Clear callbacks
    ClearCallbacks();

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

void RecordingController::SetRecordingOptions(const IRecordingStrategy::Options &options) {
    if (m_Strategy) {
        m_Strategy->SetOptions(options);
    }
}

void RecordingController::SetupInputSystemForRecording() {
    auto inputSystem = m_ServiceProvider->Resolve<InputSystem>();
    if (inputSystem) {
        inputSystem->Reset();
        inputSystem->SetEnabled(false); // Disable during recording
    }
}

void RecordingController::CleanupAfterRecording() {
    auto inputSystem = m_ServiceProvider->Resolve<InputSystem>();
    if (inputSystem) {
        inputSystem->Reset();
        inputSystem->SetEnabled(false);
    }

    auto gameInterface = m_ServiceProvider->Resolve<GameInterface>();
    if (gameInterface) {
        gameInterface->SetUIMode(UIMode::Idle);
    }
}

void RecordingController::SetupCallbacks() {
    // Set up Time Manager callback - sets delta time during recording
    CKTimeManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!IsRecording()) {
            return;
        }

        auto recorder = m_ServiceProvider->Resolve<Recorder>();
        if (recorder) {
            auto *timeManager = static_cast<CKTimeManager *>(man);
            timeManager->SetLastDeltaTime(recorder->GetDeltaTime());
        }
    });

    // Set up Input Manager callback - records input each frame
    CKInputManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!IsRecording()) {
            return;
        }

        try {
            auto recorder = m_ServiceProvider->Resolve<Recorder>();
            if (recorder) {
                auto *inputManager = static_cast<CKInputManager *>(man);
                recorder->Tick(m_CurrentTick, inputManager->GetKeyboardState());
            }

            IncrementTick();
        } catch (const std::exception &e) {
            Log::Error("Recording callback error: %s", e.what());
        }
    });

    Log::Info("RecordingController: Callbacks set up");
}

void RecordingController::ClearCallbacks() {
    CKTimeManagerHook::ClearPostCallbacks();
    CKInputManagerHook::ClearPostCallbacks();
    Log::Info("RecordingController: Callbacks cleared");
}

// ============================================================================
// PlaybackController Implementation
// ============================================================================

PlaybackController::PlaybackController(ServiceProvider *provider)
    : m_ServiceProvider(provider), m_CurrentType(PlaybackType::None) {
    if (!m_ServiceProvider) {
        throw std::invalid_argument("ServiceProvider cannot be null");
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

Result<void> PlaybackController::StartPlayback(TASProject *project, PlaybackType type) {
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

    // Reset tick counter
    ResetTick();

    // Setup input system
    SetupInputSystemForPlayback(type);

    // Set up callbacks for playback
    SetupCallbacks(type);

    // Start playback
    auto result = m_Strategy->LoadAndPlay(project);
    if (!result.IsOk()) {
        ClearCallbacks();
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

    // Clear callbacks
    ClearCallbacks();

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
    auto inputSystem = m_ServiceProvider->Resolve<InputSystem>();
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
    auto inputSystem = m_ServiceProvider->Resolve<InputSystem>();
    if (inputSystem) {
        inputSystem->Reset();
        inputSystem->SetEnabled(false);
    }

    auto gameInterface = m_ServiceProvider->Resolve<GameInterface>();
    if (gameInterface) {
        gameInterface->SetUIMode(UIMode::Idle);
    }
}

void PlaybackController::SetupCallbacks(PlaybackType type) {
    if (type == PlaybackType::Script) {
        SetupScriptPlaybackCallbacks();
    } else if (type == PlaybackType::Record) {
        SetupRecordPlaybackCallbacks();
    }
}

void PlaybackController::ClearCallbacks() {
    CKTimeManagerHook::ClearPostCallbacks();
    CKInputManagerHook::ClearPostCallbacks();
    Log::Info("PlaybackController: Callbacks cleared");
}

void PlaybackController::SetupScriptPlaybackCallbacks() {
    auto projectManager = m_ServiceProvider->Resolve<ProjectManager>();

    CKTimeManagerHook::AddPostCallback([projectManager](CKBaseManager *man) {
        auto *timeManager = static_cast<CKTimeManager *>(man);
        TASProject *project = projectManager ? projectManager->GetCurrentProject() : nullptr;
        if (project && project->IsValid()) {
            timeManager->SetLastDeltaTime(project->GetDeltaTime());
        }
    });

    CKInputManagerHook::AddPostCallback([this](CKBaseManager *man) {
        try {
#ifdef ENABLE_REPL
            // STEP 0: REPL server tick start (process scheduled commands)
            auto replServer = m_ServiceProvider->Resolve<LuaREPLServer>();
            if (replServer && replServer->IsRunning()) {
                replServer->OnTickStart(m_CurrentTick);
                replServer->ProcessImmediateCommands();
            }
#endif

            // STEP 1: Tick all script contexts (multi-context system)
            auto scriptManager = m_ServiceProvider->Resolve<ScriptContextManager>();
            if (scriptManager) {
                scriptManager->TickAll();
            }

            // STEP 2: Apply merged inputs from all contexts
            auto *inputManager = static_cast<DX8InputManager *>(man);
            ApplyMergedContextInputs(inputManager);

            // STEP 3: Validation recording
            auto recorder = m_ServiceProvider->Resolve<Recorder>();
            if (recorder && recorder->IsRecording()) {
                auto *inputMgr = static_cast<CKInputManager *>(man);
                recorder->Tick(m_CurrentTick, inputMgr->GetKeyboardState());
            }

            // STEP 4: Increment frame counter for next iteration
            IncrementTick();

#ifdef ENABLE_REPL
            // STEP 5: REPL server tick end (send notifications)
            if (replServer && replServer->IsRunning()) {
                replServer->OnTickEnd(m_CurrentTick);
            }
#endif
        } catch (const std::exception &e) {
            Log::Error("Script playback callback error: %s", e.what());
        }
    });

    Log::Info("PlaybackController: Script playback callbacks set up");
}

void PlaybackController::SetupRecordPlaybackCallbacks() {
    auto recordPlayer = m_ServiceProvider->Resolve<RecordPlayer>();

    // TimeManager callback: Set delta time from record data
    CKTimeManagerHook::AddPostCallback([this, recordPlayer](CKBaseManager *man) {
        if (recordPlayer && recordPlayer->IsPlaying()) {
            auto *timeManager = static_cast<CKTimeManager *>(man);
            // Set delta time from the current frame in the record
            float deltaTime = recordPlayer->GetFrameDeltaTime(m_CurrentTick);
            timeManager->SetLastDeltaTime(deltaTime);
        }
    });

    // InputManager callback: Apply keyboard state and advance frame
    CKInputManagerHook::AddPostCallback([this, recordPlayer](CKBaseManager *man) {
        if (recordPlayer && recordPlayer->IsPlaying()) {
            try {
                auto *inputManager = static_cast<CKInputManager *>(man);

                // This will apply the current frame's input and advance to the next frame
                recordPlayer->Tick(m_CurrentTick, inputManager->GetKeyboardState());

                IncrementTick();
            } catch (const std::exception &e) {
                Log::Error("Record playback callback error: %s", e.what());
                // Stop on error
                if (recordPlayer) {
                    recordPlayer->Stop();
                }
            }
        }
    });

    Log::Info("PlaybackController: Record playback callbacks set up");
}

void PlaybackController::ApplyMergedContextInputs(DX8InputManager *inputManager) {
    auto scriptManager = m_ServiceProvider->Resolve<ScriptContextManager>();
    if (!inputManager || !scriptManager) {
        return;
    }

    // Get all active contexts sorted by priority (highest first)
    auto contexts = scriptManager->GetContextsByPriority();

    // If no contexts are executing, nothing to apply
    if (contexts.empty()) {
        return;
    }

    // Collect inputs from all executing contexts
    std::vector<const InputSystem *> activeInputs;
    std::vector<std::string> contextNames; // For conflict logging

    for (const auto &context : contexts) {
        if (context && context->IsExecuting()) {
            const InputSystem *inputSys = context->GetInputSystem();
            if (inputSys) {
                if (inputSys->IsEnabled()) {
                    activeInputs.push_back(inputSys);
                    contextNames.push_back(context->GetName());
                }
            } else {
                // Warn if an executing context has no InputSystem (initialization issue)
                Log::Warn("Context '%s' is executing but has no InputSystem.",
                          context->GetName().c_str());
            }
        }
    }

    // If no active inputs, nothing to apply
    if (activeInputs.empty()) {
        return;
    }

    // === Priority-Based Merging Strategy ===
    // Process from lowest to highest priority (so highest priority wins)
    for (size_t i = activeInputs.size(); i > 0; --i) {
        InputSystem *inputSys = const_cast<InputSystem *>(activeInputs[i - 1]);
        inputSys->Apply(m_CurrentTick, inputManager);
    }
}

Result<std::unique_ptr<IPlaybackStrategy>> PlaybackController::CreateStrategy(PlaybackType type) {
    if (type == PlaybackType::Script) {
        auto strategy = std::make_unique<ScriptPlaybackStrategy>(m_ServiceProvider);
        auto result = strategy->Initialize();
        if (!result.IsOk()) {
            return Result<std::unique_ptr<IPlaybackStrategy>>::Error(result.GetError());
        }
        return Result<std::unique_ptr<IPlaybackStrategy>>::Ok(std::move(strategy));
    } else if (type == PlaybackType::Record) {
        auto strategy = std::make_unique<RecordPlaybackStrategy>(m_ServiceProvider);
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

TranslationController::TranslationController(ServiceProvider *provider)
    : m_ServiceProvider(provider) {
    if (!m_ServiceProvider) {
        throw std::invalid_argument("ServiceProvider cannot be null");
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

Result<void> TranslationController::StartTranslation(TASProject *project,
                                                     const GenerationOptions &options) {
    if (!m_IsInitialized) {
        return Result<void>::Error("TranslationController not initialized", "state");
    }

    if (!project) {
        return Result<void>::Error("Project cannot be null", "invalid_argument");
    }

    if (m_IsTranslating) {
        return Result<void>::Error("Already translating", "state");
    }

    auto recorder = m_ServiceProvider->Resolve<Recorder>();
    auto recordPlayer = m_ServiceProvider->Resolve<RecordPlayer>();

    if (!recorder || !recordPlayer) {
        return Result<void>::Error("Recorder or RecordPlayer not available", "subsystem");
    }

    // Store generation options
    m_GenerationOptions = options;

    // Reset tick counter
    ResetTick();

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
        recorder->Stop(); // Clean up
        return Result<void>::Error("Failed to start record playback for translation", "playback");
    }

    // Set up callbacks for translation
    SetupCallbacks();

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
    auto recordPlayer = m_ServiceProvider->Resolve<RecordPlayer>();
    if (recordPlayer) {
        recordPlayer->Stop();
    }

    // Stop recorder (will auto-generate script if configured)
    auto recorder = m_ServiceProvider->Resolve<Recorder>();
    if (recorder && recorder->IsRecording()) {
        recorder->Stop();
        Log::Info("TranslationController: Recorder stopped, script generated");
    }

    // Clear callbacks
    ClearCallbacks();

    m_IsTranslating = false;

    if (clearProject) {
        m_CurrentProject = nullptr;
    }

    auto gameInterface = m_ServiceProvider->Resolve<GameInterface>();
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

    auto recordPlayer = m_ServiceProvider->Resolve<RecordPlayer>();
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

    Log::Info("Record playback completed during translation. Generating script...");

    // Stop translation (Recorder will auto-generate script)
    StopTranslation(false);
}

void TranslationController::SetupCallbacks() {
    auto projectManager = m_ServiceProvider->Resolve<ProjectManager>();

    // TimeManager callback: Use delta time from record data
    CKTimeManagerHook::AddPostCallback([this, projectManager](CKBaseManager *man) {
        if (!IsTranslating()) {
            return;
        }

        auto *timeManager = static_cast<CKTimeManager *>(man);
        TASProject *project = projectManager ? projectManager->GetCurrentProject() : nullptr;
        if (project && project->IsValid()) {
            timeManager->SetLastDeltaTime(project->GetDeltaTime());
        }
    });

    // InputManager callback: Apply record input and capture it for recording
    CKInputManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!IsTranslating()) {
            return;
        }

        try {
            auto *inputManager = static_cast<CKInputManager *>(man);
            unsigned char *keyboardState = inputManager->GetKeyboardState();

            auto recordPlayer = m_ServiceProvider->Resolve<RecordPlayer>();
            auto recorder = m_ServiceProvider->Resolve<Recorder>();

            // STEP 1: Apply input from record player
            if (recordPlayer && recordPlayer->IsPlaying()) {
                recordPlayer->Tick(m_CurrentTick, keyboardState);
            }

            // STEP 2: Capture the applied input with recorder
            if (recorder && recorder->IsRecording()) {
                recorder->Tick(m_CurrentTick, keyboardState);
            }

            // STEP 3: Increment the current tick for next frame
            IncrementTick();

            // STEP 4: Check if record playback has finished
            if (recordPlayer && !recordPlayer->IsPlaying() && IsTranslating()) {
                // Record playback completed, finish translation
                OnTranslationPlaybackComplete();
            }
        } catch (const std::exception &e) {
            Log::Error("Translation callback error: %s", e.what());
            StopTranslation();
        }
    });

    Log::Info("TranslationController: Callbacks set up");
}

void TranslationController::ClearCallbacks() {
    CKTimeManagerHook::ClearPostCallbacks();
    CKInputManagerHook::ClearPostCallbacks();
    Log::Info("TranslationController: Callbacks cleared");
}
