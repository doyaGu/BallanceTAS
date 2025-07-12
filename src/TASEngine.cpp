#include "TASEngine.h"

#include "GameInterface.h"
#include "ProjectManager.h"
#include "InputSystem.h"
#include "EventManager.h"
#include "TASHook.h"
#include "TASProject.h"
#include "Recorder.h"
#include "ScriptGenerator.h"
#include "ScriptExecutor.h"
#include "RecordPlayer.h"
#include "UIManager.h"

TASEngine::TASEngine(GameInterface *game) : m_GameInterface(game), m_ShuttingDown(false) {
    if (!m_GameInterface) {
        throw std::runtime_error("TASEngine requires a valid GameInterface instance.");
    }
}

TASEngine::~TASEngine() {
    // Ensure shutdown is called
    if (!m_ShuttingDown) {
        Shutdown();
    }
}

bool TASEngine::Initialize() {
    if (m_ShuttingDown) {
        GetLogger()->Error("Cannot initialize TASEngine during shutdown.");
        return false;
    }

    // 1. Create Core Subsystems
    try {
        m_InputSystem = std::make_unique<InputSystem>();
        m_EventManager = std::make_unique<EventManager>(this);

        // Initialize recording subsystems
        m_Recorder = std::make_unique<Recorder>(this);
        m_ScriptGenerator = std::make_unique<ScriptGenerator>(this);

        // Initialize execution subsystems
        m_ScriptExecutor = std::make_unique<ScriptExecutor>(this);
        m_RecordPlayer = std::make_unique<RecordPlayer>(this);
    } catch (const std::exception &e) {
        GetLogger()->Error("Failed to initialize core subsystems: %s", e.what());
        return false;
    }

    // 2. Initialize execution subsystems
    try {
        if (!m_ScriptExecutor->Initialize()) {
            GetLogger()->Error("Failed to initialize ScriptExecutor.");
            return false;
        }
    } catch (const std::exception &e) {
        GetLogger()->Error("Failed to initialize execution subsystems: %s", e.what());
        return false;
    }

    // 3. Initialize Project Manager (needs ScriptExecutor's Lua state)
    try {
        m_ProjectManager = std::make_unique<ProjectManager>(this);
    } catch (const std::exception &e) {
        GetLogger()->Error("Failed to initialize project manager: %s", e.what());
        return false;
    }

    // 4. Set up recording callbacks
    if (m_Recorder) {
        m_Recorder->SetStatusCallback([this](bool isRecording) {
            if (m_ShuttingDown) return;

            if (isRecording) {
                m_State |= TAS_RECORDING;
                m_State &= ~(TAS_PLAYING | TAS_PLAY_PENDING); // Can't record and play simultaneously
            } else {
                m_State &= ~TAS_RECORDING;
            }
            m_GameInterface->SetUIMode(isRecording ? UIMode::Recording : UIMode::Idle);
        });
    }

    GetLogger()->Info("TASEngine and all subsystems initialized.");
    return true;
}

void TASEngine::Shutdown() {
    if (m_ShuttingDown) {
        return; // Already shutting down
    }

    m_ShuttingDown = true;

    try {
        // Clear callbacks first to prevent any timer callbacks from firing
        ClearCallbacks();

        // Stop any active recording or playback immediately (without timers)
        if (IsRecording()) {
            StopRecordingImmediate();
        }
        if (IsPlaying()) {
            StopReplayImmediate();
        }

        // Shutdown in reverse order
        m_ProjectManager.reset();
        m_ScriptGenerator.reset();
        m_Recorder.reset();

        // Shutdown execution subsystems
        if (m_ScriptExecutor) {
            m_ScriptExecutor->Shutdown();
            m_ScriptExecutor.reset();
        }
        m_RecordPlayer.reset();

        m_EventManager.reset();
        m_InputSystem.reset();

        GetLogger()->Info("TASEngine shutdown complete.");
    } catch (const std::exception &e) {
        GetLogger()->Error("Exception during TASEngine shutdown: %s", e.what());
    }
}

void TASEngine::Start() {
    if (m_ShuttingDown || (m_State & (TAS_PLAYING | TAS_RECORDING))) {
        return; // Already active or shutting down
    }

    AddTimer(1ul, [this]() {
        if (m_GameInterface) {
            m_GameInterface->ResetPhysicsTime();
        }
    });

    // Check if we should start recording instead of playing
    if (IsPendingRecord()) {
        StartRecordingInternal();
        return;
    }

    // Check if we should start playing
    if (IsPendingPlay()) {
        StartReplayInternal();
        return;
    }
}

void TASEngine::Stop() {
    if (m_ShuttingDown) {
        return;
    }

    if (IsRecording()) {
        StopRecording();
    } else if (IsPlaying()) {
        StopReplay();
    } else {
        // Stop any pending operations
        m_State = TAS_IDLE;
        m_PlaybackType = PlaybackType::None;
        m_GameInterface->SetUIMode(UIMode::Idle);
    }

    if (!m_GameInterface->IsLegacyMode()) {
        AddTimer(1ul, [this]() {
            if (m_GameInterface) {
                m_GameInterface->SetPhysicsTimeFactor();
            }
        });
    }
}

// === Recording Control ===

bool TASEngine::StartRecording() {
    if (m_ShuttingDown || IsRecording() || IsPlaying() || IsPendingPlay()) {
        GetLogger()->Warn("Cannot start recording: TAS is already active or shutting down.");
        return false;
    }

    if (!m_Recorder) {
        GetLogger()->Error("Recorder not initialized.");
        return false;
    }

    SetRecordPending(true);
    GetLogger()->Info("Recording setup complete. Will start when level loads.");
    return true;
}

void TASEngine::StopRecording() {
    if (m_ShuttingDown) {
        StopRecordingImmediate();
        return;
    }

    if (!IsRecording() && !IsPendingRecord()) {
        return;
    }

    try {
        if (IsRecording() && m_Recorder) {
            m_Recorder->Stop(); // This will auto-generate if configured
        }
    } catch (const std::exception &e) {
        GetLogger()->Error("Exception stopping recording: %s", e.what());
    }

    // Ensure InputSystem remains disabled after recording
    if (m_InputSystem) {
        m_InputSystem->ReleaseAllKeys();
        m_InputSystem->SetEnabled(false);
    }

    ClearCallbacks();
    SetRecording(false);
    SetRecordPending(false);

    m_GameInterface->SetUIMode(UIMode::Idle);
    GetLogger()->Info("Recording stopped.");
}

void TASEngine::StopRecordingImmediate() {
    try {
        if (IsRecording() && m_Recorder) {
            m_Recorder->Stop();
        }

        SetRecording(false);
        SetRecordPending(false);

        m_GameInterface->SetUIMode(UIMode::Idle);
        GetLogger()->Info("Recording stopped immediately.");
    } catch (const std::exception &e) {
        GetLogger()->Error("Exception during immediate recording stop: %s", e.what());
    }
}

size_t TASEngine::GetRecordingFrameCount() const {
    if (!IsRecording() || !m_Recorder) {
        return 0;
    }
    return m_Recorder->GetTotalFrames();
}

// === Unified Replay Control ===

bool TASEngine::StartReplay() {
    if (m_ShuttingDown || IsPlaying() || IsRecording() || IsPendingRecord()) {
        GetLogger()->Warn("Cannot start replay: TAS is already active or shutting down.");
        return false;
    }

    TASProject *project = m_ProjectManager->GetCurrentProject();
    if (!project || !project->IsValid()) {
        GetLogger()->Error("No valid TAS project selected.");
        return false;
    }

    // Check compatibility for record projects
    if (project->IsRecordProject() && !m_GameInterface->IsLegacyMode()) {
        GetLogger()->Error("Record playback requires legacy mode to be enabled.");
        return false;
    }

    SetPlayPending(true);
    GetLogger()->Info("Replay setup complete. Will start when level loads.");
    return true;
}

void TASEngine::StopReplay() {
    if (m_ShuttingDown) {
        StopReplayImmediate();
        return;
    }

    if (!IsPlaying() && !IsPendingPlay()) {
        return;
    }

    try {
        // Stop the appropriate executor
        if (m_PlaybackType == PlaybackType::Script && m_ScriptExecutor) {
            m_ScriptExecutor->Stop();
        } else if (m_PlaybackType == PlaybackType::Record && m_RecordPlayer) {
            m_RecordPlayer->Stop();
        }
    } catch (const std::exception &e) {
        GetLogger()->Error("Exception stopping replay: %s", e.what());
    }

    // Clean up input state based on playback type
    if (m_InputSystem) {
        if (m_PlaybackType == PlaybackType::Script) {
            // For script playback, InputSystem was enabled - disable it and clean up
            m_InputSystem->ReleaseAllKeys();
            m_InputSystem->SetEnabled(false);
        }
        // For record playback, InputSystem was already disabled, but ensure keys are clean
        // RecordPlayer handles its own keyboard state cleanup
    }

    ClearCallbacks();
    SetPlaying(false);
    SetPlayPending(false);

    // Reset keyboard state to ensure clean state
    memset(m_GameInterface->GetInputManager()->GetKeyboardState(), KS_IDLE, 256);

    m_GameInterface->SetUIMode(UIMode::Idle);
    GetLogger()->Info("Replay stopped.");
}

void TASEngine::StopReplayImmediate() {
    try {
        // Stop both executors
        if (m_ScriptExecutor) {
            m_ScriptExecutor->Stop();
        }
        if (m_RecordPlayer) {
            m_RecordPlayer->Stop();
        }

        // Immediately disable InputSystem and clean up
        if (m_InputSystem) {
            m_InputSystem->ReleaseAllKeys();
            m_InputSystem->SetEnabled(false);
        }

        SetPlaying(false);
        SetPlayPending(false);

        // Reset keyboard state to ensure clean state
        memset(m_GameInterface->GetInputManager()->GetKeyboardState(), KS_IDLE, 256);

        m_GameInterface->SetUIMode(UIMode::Idle);
        GetLogger()->Info("Replay stopped immediately.");
    } catch (const std::exception &e) {
        GetLogger()->Error("Exception during immediate replay stop: %s", e.what());
    }
}

// === Internal Start Methods ===

void TASEngine::StartRecordingInternal() {
    if (m_ShuttingDown) {
        return;
    }

    // Clear any existing callbacks first to prevent duplicates
    ClearCallbacks();

    // Set up callbacks for recording mode
    SetupRecordingCallbacks();

    SetCurrentTick(0);

    if (m_GameInterface) {
        m_GameInterface->AcquireKeyBindings();
    }

    // Ensure InputSystem is DISABLED during recording
    // We want to capture the user's actual input, not override it
    if (m_InputSystem) {
        m_InputSystem->ReleaseAllKeys(); // Start with clean state
        m_InputSystem->SetEnabled(false);
    }

    try {
        if (m_Recorder && !m_Recorder->IsRecording()) {
            m_Recorder->Start();
        }
    } catch (const std::exception &e) {
        GetLogger()->Error("Exception during recording start: %s", e.what());
        Stop();
    }

    SetRecordPending(false);
    SetRecording(true);

    GetLogger()->Info("Started recording new TAS.");
}

void TASEngine::StartReplayInternal() {
    if (m_ShuttingDown) {
        return;
    }

    TASProject *project = m_ProjectManager->GetCurrentProject();
    if (!project || !project->IsValid()) {
        GetLogger()->Error("No valid TAS project selected.");
        Stop();
        return;
    }

    // Determine playback type
    PlaybackType playbackType = DeterminePlaybackType(project);
    if (playbackType == PlaybackType::None) {
        GetLogger()->Error("Unable to determine playback type for project: %s", project->GetName().c_str());
        Stop();
        return;
    }

    // Clear any existing callbacks first to prevent duplicates
    ClearCallbacks();

    // Set up appropriate callbacks based on playback type
    if (playbackType == PlaybackType::Script) {
        SetupScriptPlaybackCallbacks();
    } else if (playbackType == PlaybackType::Record) {
        SetupRecordPlaybackCallbacks();
    }

    SetCurrentTick(0);

    if (m_GameInterface) {
        m_GameInterface->AcquireKeyBindings();
    }

    // Handle InputSystem differently for different playback types
    if (m_InputSystem) {
        if (playbackType == PlaybackType::Script) {
            // For script playback, enable InputSystem for deterministic replay
            m_InputSystem->SetEnabled(true);
            m_InputSystem->ReleaseAllKeys(); // Start with clean state
        } else if (playbackType == PlaybackType::Record) {
            // For record playback, DISABLE InputSystem completely
            // Record playback applies input directly to keyboard state buffer
            m_InputSystem->SetEnabled(false);
            m_InputSystem->ReleaseAllKeys(); // Ensure clean state
        }
    }

    try {
        bool success = false;

        if (playbackType == PlaybackType::Script && m_ScriptExecutor) {
            success = m_ScriptExecutor->LoadAndExecute(project);
        } else if (playbackType == PlaybackType::Record && m_RecordPlayer) {
            success = m_RecordPlayer->LoadAndPlay(project);
        }

        if (!success) {
            GetLogger()->Error("Failed to start TAS project playback.");
            Stop();
            return;
        }
    } catch (const std::exception &e) {
        GetLogger()->Error("Exception during TAS start: %s", e.what());
        Stop();
        return;
    }

    SetPlayPending(false);
    SetPlaying(true);
    m_PlaybackType = playbackType;

    m_GameInterface->SetUIMode(UIMode::Playing);
    GetLogger()->Info("Started playing TAS project: %s (%s mode)",
                      project->GetName().c_str(),
                      playbackType == PlaybackType::Script ? "Script" : "Record");
}

PlaybackType TASEngine::DeterminePlaybackType(const TASProject *project) const {
    if (!project || !project->IsValid()) {
        return PlaybackType::None;
    }

    if (project->IsScriptProject()) {
        return PlaybackType::Script;
    } else if (project->IsRecordProject()) {
        return PlaybackType::Record;
    }

    return PlaybackType::None;
}

size_t TASEngine::GetCurrentTick() const {
    return m_CurrentTick;
}

void TASEngine::SetCurrentTick(size_t tick) {
    m_CurrentTick = tick;
}

void TASEngine::ClearCallbacks() {
    CKTimeManagerHook::ClearPostCallbacks();
    CKInputManagerHook::ClearPostCallbacks();
}

void TASEngine::SetupRecordingCallbacks() {
    if (m_ShuttingDown) {
        return;
    }

    CKTimeManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!m_ShuttingDown) {
            auto *timeManager = static_cast<CKTimeManager *>(man);
            if (IsRecording() && m_Recorder) {
                timeManager->SetLastDeltaTime(m_Recorder->GetDeltaTime());
            }
        }
    });

    CKInputManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!m_ShuttingDown) {
            try {
                if (IsRecording()) {
                    // This captures the exact accumulated state that the game will see
                    if (m_Recorder) {
                        auto *inputManager = static_cast<CKInputManager *>(man);
                        m_Recorder->Tick(m_CurrentTick, inputManager->GetKeyboardState());
                    }
                }
            } catch (const std::exception &e) {
                GetLogger()->Error("Recording callback error: %s", e.what());
            }
        }
    });
}

void TASEngine::SetupScriptPlaybackCallbacks() {
    if (m_ShuttingDown) {
        return;
    }

    CKTimeManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!m_ShuttingDown) {
            auto *timeManager = static_cast<CKTimeManager *>(man);
            TASProject *project = m_ProjectManager->GetCurrentProject();
            if (project && project->IsValid()) {
                timeManager->SetLastDeltaTime(project->GetDeltaTime());
            }
        }
    });

    CKInputManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!m_ShuttingDown) {
            try {
                size_t currentTick = GetCurrentTick();

                // STEP 1: Process script execution
                if (m_ScriptExecutor) {
                    m_ScriptExecutor->Tick();
                }

                // STEP 2: Apply InputSystem changes
                if (m_InputSystem && m_InputSystem->IsEnabled()) {
                    auto *inputManager = static_cast<CKInputManager *>(man);
                    m_InputSystem->Apply(currentTick, inputManager->GetKeyboardState());
                }

                // STEP 3: Increment frame counter for next iteration
                IncrementCurrentTick();

                // STEP 4: Prepare InputSystem for next frame
                if (m_InputSystem && m_InputSystem->IsEnabled()) {
                    m_InputSystem->PrepareNextFrame();
                }
            } catch (const std::exception &e) {
                GetLogger()->Error("Script playback callback error: %s", e.what());
            }
        }
    });
}

void TASEngine::SetupRecordPlaybackCallbacks() {
    if (m_ShuttingDown) {
        return;
    }

    // TimeManager callback: Set delta time from record data
    CKTimeManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!m_ShuttingDown && m_RecordPlayer && m_RecordPlayer->IsPlaying()) {
            auto *timeManager = static_cast<CKTimeManager *>(man);
            // Set delta time from the current frame in the record
            float deltaTime = m_RecordPlayer->GetFrameDeltaTime(m_CurrentTick);
            timeManager->SetLastDeltaTime(deltaTime);
        }
    });

    // InputManager callback: Apply keyboard state and advance frame
    CKInputManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!m_ShuttingDown && m_RecordPlayer && m_RecordPlayer->IsPlaying()) {
            try {
                auto *inputManager = static_cast<CKInputManager *>(man);

                // This will apply the current frame's input and advance to the next frame
                m_RecordPlayer->Tick(m_CurrentTick, inputManager->GetKeyboardState());
            } catch (const std::exception &e) {
                GetLogger()->Error("Record playback callback error: %s", e.what());
                // Stop on error
                if (m_RecordPlayer) {
                    m_RecordPlayer->Stop();
                }
            }
        }
    });
}

template <typename... Args>
void TASEngine::OnGameEvent(const std::string &eventName, Args... args) {
    if (m_ShuttingDown) {
        return;
    }

    // Forward to script executor for script-based events
    if (m_ScriptExecutor) {
        m_ScriptExecutor->FireGameEvent(eventName, args...);
    }

    // Forward to recorder if recording
    if (IsRecording() && m_Recorder) {
        if constexpr (sizeof...(args) > 0) {
            // If there are arguments, pass the first one as event data
            auto firstArg = std::get<0>(std::make_tuple(args...));
            if constexpr (std::is_convertible_v<decltype(firstArg), int>) {
                m_Recorder->OnGameEvent(m_CurrentTick, eventName, static_cast<int>(firstArg));
            } else {
                m_Recorder->OnGameEvent(m_CurrentTick, eventName, 0);
            }
        } else {
            m_Recorder->OnGameEvent(m_CurrentTick, eventName, 0);
        }
    }
}

// Explicit template instantiations for events used in BallanceTAS.cpp
template void TASEngine::OnGameEvent(const std::string &);
template void TASEngine::OnGameEvent(const std::string &, int);

ILogger *TASEngine::GetLogger() const { return m_GameInterface->GetLogger(); }

void TASEngine::AddTimer(size_t tick, const std::function<void()> &callback) {
    m_GameInterface->AddTimer(tick, callback);
}

// === Lua API Compatibility Delegates ===

sol::state &TASEngine::GetLuaState() {
    if (!m_ScriptExecutor) {
        throw std::runtime_error("ScriptExecutor not initialized");
    }
    return m_ScriptExecutor->GetLuaState();
}

const sol::state &TASEngine::GetLuaState() const {
    if (!m_ScriptExecutor) {
        throw std::runtime_error("ScriptExecutor not initialized");
    }
    return m_ScriptExecutor->GetLuaState();
}

LuaScheduler *TASEngine::GetScheduler() const {
    if (!m_ScriptExecutor) {
        return nullptr;
    }
    return m_ScriptExecutor->GetScheduler();
}
