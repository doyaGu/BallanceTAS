#include "TASEngine.h"

#include "BallanceTAS.h"
#include "LuaApi.h"
#include "LuaScheduler.h"
#include "ProjectManager.h"
#include "InputSystem.h"
#include "GameInterface.h"
#include "DevTools.h"
#include "EventManager.h"
#include "TASHook.h"
#include "TASProject.h"
#include "Recorder.h"
#include "ScriptGenerator.h"

TASEngine::TASEngine(BallanceTAS *mod) : m_Mod(mod), m_ShuttingDown(false) {}

TASEngine::~TASEngine() {
    // Ensure shutdown is called
    if (!m_ShuttingDown) {
        Shutdown();
    }
}

bool TASEngine::Initialize() {
    if (m_ShuttingDown) {
        m_Mod->GetLogger()->Error("Cannot initialize TASEngine during shutdown.");
        return false;
    }

    // 1. Initialize Lua State
    try {
        m_LuaState.open_libraries(
            sol::lib::base,
            sol::lib::package,
            sol::lib::coroutine,
            sol::lib::string,
            sol::lib::os,
            sol::lib::math,
            sol::lib::table,
            sol::lib::debug,
            sol::lib::io // Potentially restrict this for security later
        );
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Failed to initialize Lua state: %s", e.what());
        return false;
    }

    // 2. Create Core Subsystems (order can be important)
    try {
        m_InputSystem = std::make_unique<InputSystem>();
        m_GameInterface = std::make_unique<GameInterface>(m_Mod);
        m_Scheduler = std::make_unique<LuaScheduler>(this);
        m_DevTools = std::make_unique<DevTools>(m_Mod->GetBML());
        m_EventManager = std::make_unique<EventManager>(*m_Scheduler);

        // Initialize recording subsystems
        m_Recorder = std::make_unique<Recorder>(this);
        m_ScriptGenerator = std::make_unique<ScriptGenerator>(this);
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Failed to initialize core subsystems: %s", e.what());
        return false;
    }

    // 3. Register Lua APIs
    // Pass 'this' to give the API layer access to all subsystems.
    try {
        LuaApi::Register(this);
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Failed to register Lua APIs: %s", e.what());
        return false;
    }

    // 4. Initialize Project Manager
    try {
        m_ProjectManager = std::make_unique<ProjectManager>(m_Mod, m_LuaState);
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Failed to initialize project manager: %s", e.what());
        return false;
    }

    // 5. Set up recording callbacks
    if (m_Recorder) {
        m_Recorder->SetStatusCallback([this](bool isRecording) {
            if (m_ShuttingDown) return;

            if (isRecording) {
                m_State |= TAS_RECORDING;
                m_State &= ~(TAS_PLAYING | TAS_PLAY_PENDING); // Can't record and play simultaneously
            } else {
                m_State &= ~TAS_RECORDING;
            }
            if (m_Mod) {
                m_Mod->SetUIMode(isRecording ? 2 : 0); // 2 = Recording mode, 0 = Idle
            }
        });
    }

    m_Mod->GetLogger()->Info("TASEngine and all subsystems initialized.");
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
        m_EventManager.reset();
        m_DevTools.reset();
        m_Scheduler.reset();
        m_GameInterface.reset();
        m_InputSystem.reset();

        m_Mod->GetLogger()->Info("TASEngine shutdown complete.");
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Exception during TASEngine shutdown: %s", e.what());
    }
}

void TASEngine::Start() {
    if (m_ShuttingDown || (m_State & (TAS_PLAYING | TAS_RECORDING))) {
        return; // Already active or shutting down
    }

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
        if (m_Mod) {
            m_Mod->SetUIMode(0); // UIMode::Idle
        }
    }
}

// === Recording Control ===

bool TASEngine::StartRecording() {
    if (m_ShuttingDown || IsRecording() || IsPlaying() || IsPendingPlay()) {
        m_Mod->GetLogger()->Warn("Cannot start recording: TAS is already active or shutting down.");
        return false;
    }

    if (!m_Recorder) {
        m_Mod->GetLogger()->Error("Recorder not initialized.");
        return false;
    }

    SetRecordPending(true);
    m_Mod->GetLogger()->Info("Recording setup complete. Will start when level loads.");
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

        // Ensure InputSystem remains disabled after recording
        if (m_InputSystem) {
            m_InputSystem->ReleaseAllKeys();
            m_InputSystem->SetEnabled(false);
        }

        ClearCallbacks();
        SetRecording(false);
        SetRecordPending(false);

        // Use a safer timer approach
        if (m_Mod && m_Mod->GetBML()) {
            m_Mod->GetBML()->AddTimer(1ul, [this]() {
                if (!m_ShuttingDown && m_GameInterface && m_Mod) {
                    try {
                        m_GameInterface->SetPhysicsTimeFactor();
                        m_GameInterface->SetCurrentTick(0);
                        m_Mod->SetUIMode(0); // UIMode::Idle
                        m_Mod->GetLogger()->Info("Recording stopped.");
                    } catch (const std::exception &e) {
                        if (m_Mod && m_Mod->GetLogger()) {
                            m_Mod->GetLogger()->Error("Exception during recording stop: %s", e.what());
                        }
                    }
                }
            });
        }
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Exception stopping recording: %s", e.what());
        SetRecording(false);
        SetRecordPending(false);

        // Ensure InputSystem is disabled even on error
        if (m_InputSystem) {
            m_InputSystem->ReleaseAllKeys();
            m_InputSystem->SetEnabled(false);
        }
    }
}

void TASEngine::StopRecordingImmediate() {
    try {
        if (IsRecording() && m_Recorder) {
            m_Recorder->Stop();
        }

        SetRecording(false);
        SetRecordPending(false);

        if (m_GameInterface) {
            m_GameInterface->SetPhysicsTimeFactor();
            m_GameInterface->SetCurrentTick(0);
        }

        if (m_Mod) {
            m_Mod->SetUIMode(0); // UIMode::Idle
            m_Mod->GetLogger()->Info("Recording stopped immediately.");
        }
    } catch (const std::exception &e) {
        if (m_Mod && m_Mod->GetLogger()) {
            m_Mod->GetLogger()->Error("Exception during immediate recording stop: %s", e.what());
        }
    }
}

// === Replay Control ===

bool TASEngine::StartReplay() {
    if (m_ShuttingDown || IsPlaying() || IsRecording() || IsPendingRecord()) {
        m_Mod->GetLogger()->Warn("Cannot start replay: TAS is already active or shutting down.");
        return false;
    }

    TASProject *project = m_ProjectManager->GetCurrentProject();
    if (!project || !project->IsValid()) {
        m_Mod->GetLogger()->Error("No valid TAS project selected.");
        return false;
    }

    SetPlayPending(true);
    m_Mod->GetLogger()->Info("Replay setup complete. Will start when level loads.");
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
        if (m_Scheduler) {
            m_Scheduler->Clear();
        }

        // Disable InputSystem and clean up input state
        if (m_InputSystem) {
            m_InputSystem->ReleaseAllKeys();
            m_InputSystem->SetEnabled(false);
        }

        ClearCallbacks();
        SetPlaying(false);
        SetPlayPending(false);

        // Use a safer timer approach
        if (m_Mod && m_Mod->GetBML()) {
            InputSystem::Reset(m_Mod->GetInputManager()->GetKeyboardState());

            m_Mod->GetBML()->AddTimer(1ul, [this]() {
                if (!m_ShuttingDown && m_GameInterface && m_Mod) {
                    try {
                        m_GameInterface->SetPhysicsTimeFactor();
                        m_GameInterface->SetCurrentTick(0);
                        m_Mod->SetUIMode(0); // UIMode::Idle
                        m_Mod->GetLogger()->Info("Replay stopped.");
                    } catch (const std::exception &e) {
                        if (m_Mod && m_Mod->GetLogger()) {
                            m_Mod->GetLogger()->Error("Exception during replay stop: %s", e.what());
                        }
                    }
                }
            });
        }
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Exception stopping replay: %s", e.what());
        SetPlaying(false);
        SetPlayPending(false);

        // Ensure input control is disabled even on error
        if (m_InputSystem) {
            m_InputSystem->ReleaseAllKeys();
            m_InputSystem->SetEnabled(false);
        }
    }
}

void TASEngine::StopReplayImmediate() {
    try {
        if (m_Scheduler) {
            m_Scheduler->Clear();
        }

        // Immediately disable InputSystem
        if (m_InputSystem) {
            m_InputSystem->ReleaseAllKeys();
            m_InputSystem->SetEnabled(false);
        }

        SetPlaying(false);
        SetPlayPending(false);

        if (m_GameInterface) {
            m_GameInterface->SetPhysicsTimeFactor();
            m_GameInterface->SetCurrentTick(0);
        }

        if (m_Mod) {
            InputSystem::Reset(m_Mod->GetInputManager()->GetKeyboardState());

            m_Mod->SetUIMode(0); // UIMode::Idle
            m_Mod->GetLogger()->Info("Replay stopped immediately.");
        }
    } catch (const std::exception &e) {
        if (m_Mod && m_Mod->GetLogger()) {
            m_Mod->GetLogger()->Error("Exception during immediate replay stop: %s", e.what());
        }
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

    if (m_GameInterface) {
        m_GameInterface->AcquireKeyBindings();
    }

    // Ensure InputSystem is DISABLED during recording
    // We want to capture the user's actual input, not override it
    if (m_InputSystem) {
        m_InputSystem->SetEnabled(false);
        m_InputSystem->ReleaseAllKeys(); // Start with clean state
    }

    if (m_Mod && m_Mod->GetBML()) {
        m_Mod->GetBML()->AddTimer(1ul, [this]() {
            if (!m_ShuttingDown) {
                try {
                    if (m_GameInterface) {
                        m_GameInterface->ResetPhysicsTime();
                        m_GameInterface->SetCurrentTick(0);
                    }

                    if (m_Recorder && !m_Recorder->IsRecording()) {
                        m_Recorder->Start();
                    }

                    SetRecordPending(false);
                    SetRecording(true);

                    if (m_Mod) {
                        m_Mod->GetLogger()->Info("Started recording new TAS.");
                    }
                } catch (const std::exception &e) {
                    if (m_Mod) {
                        m_Mod->GetLogger()->Error("Exception during recording start: %s", e.what());
                    }
                    Stop();
                }
            }
        });
    }
}

void TASEngine::StartReplayInternal() {
    if (m_ShuttingDown) {
        return;
    }

    TASProject *project = m_ProjectManager->GetCurrentProject();
    if (!project || !project->IsValid()) {
        m_Mod->GetLogger()->Error("No valid TAS project selected.");
        Stop();
        return;
    }

    // Clear any existing callbacks first to prevent duplicates
    ClearCallbacks();

    // Set up callbacks for playback mode
    SetupPlaybackCallbacks();

    if (m_GameInterface) {
        m_GameInterface->AcquireKeyBindings();
    }

    // Enable InputSystem for deterministic replay
    if (m_InputSystem) {
        m_InputSystem->SetEnabled(true);
        m_InputSystem->ReleaseAllKeys(); // Start with clean state
    }

    if (m_Mod && m_Mod->GetBML()) {
        m_Mod->GetBML()->AddTimer(1ul, [this, project]() {
            if (!m_ShuttingDown) {
                try {
                    if (m_GameInterface) {
                        m_GameInterface->ResetPhysicsTime();
                        m_GameInterface->SetCurrentTick(0);
                    }

                    if (!LoadTAS(project)) {
                        if (m_Mod) {
                            m_Mod->GetLogger()->Error("Failed to load TAS project.");
                        }
                        Stop();
                        return;
                    }

                    SetPlayPending(false);
                    SetPlaying(true);

                    if (m_Mod) {
                        m_Mod->SetUIMode(1); // UIMode::Playing
                        m_Mod->GetLogger()->Info("Started playing TAS project: %s", project->GetName().c_str());
                    }
                } catch (const std::exception &e) {
                    if (m_Mod) {
                        m_Mod->GetLogger()->Error("Exception during TAS start: %s", e.what());
                    }
                    Stop();
                }
            }
        });
    }
}

size_t TASEngine::GetRecordingFrameCount() const {
    if (!IsRecording() || !m_Recorder) {
        return 0;
    }
    return m_Recorder->GetTotalFrames();
}

void TASEngine::ClearCallbacks() {
    CKTimeManagerHook::ClearPostCallbacks();
    CKInputManagerHook::ClearPostCallbacks();
}

void TASEngine::SetupPlaybackCallbacks() {
    if (m_ShuttingDown) {
        return;
    }

    CKTimeManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!m_ShuttingDown) {
            try {
                auto *timeManager = static_cast<CKTimeManager *>(man);
                TASProject *project = m_ProjectManager->GetCurrentProject();
                if (project && project->IsValid()) {
                    timeManager->SetLastDeltaTime(project->GetDeltaTime());
                }
            } catch (const std::exception &e) {
                if (m_Mod) {
                    m_Mod->GetLogger()->Error("Time callback error: %s", e.what());
                }
            }
        }
    });

    CKInputManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!m_ShuttingDown) {
            try {
                if (IsPlaying() && m_GameInterface) {
                    m_GameInterface->IncrementCurrentTick();
                }

                if (m_Scheduler) {
                    m_Scheduler->Tick();
                }

                // Apply InputSystem when it's enabled
                if (m_InputSystem && m_InputSystem->IsEnabled()) {
                    auto *inputManager = static_cast<CKInputManager *>(man);
                    m_InputSystem->Apply(inputManager->GetKeyboardState());
                }
            } catch (const std::exception &e) {
                if (m_Mod) {
                    m_Mod->GetLogger()->Error("Input callback error: %s", e.what());
                }
            }
        }
    });
}

void TASEngine::SetupRecordingCallbacks() {
    if (m_ShuttingDown) {
        return;
    }

    CKTimeManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!m_ShuttingDown) {
            try {
                auto *timeManager = static_cast<CKTimeManager *>(man);
                if (IsRecording() && m_Recorder) {
                    timeManager->SetLastDeltaTime(m_Recorder->GetDeltaTime());
                }
            } catch (const std::exception &e) {
                if (m_Mod) {
                    m_Mod->GetLogger()->Error("Time callback error: %s", e.what());
                }
            }
        }
    });

    CKInputManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!m_ShuttingDown) {
            try {
                if (IsRecording()) {
                    if (m_GameInterface) {
                        m_GameInterface->IncrementCurrentTick();
                    }
                    if (m_Recorder) {
                        m_Recorder->Tick();
                    }
                }
            } catch (const std::exception &e) {
                if (m_Mod) {
                    m_Mod->GetLogger()->Error("Recording callback error: %s", e.what());
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

    if (m_EventManager) {
        m_EventManager->FireEvent(eventName, args...);
    }

    // Forward to recorder if recording
    if (IsRecording() && m_Recorder) {
        if constexpr (sizeof...(args) > 0) {
            // If there are arguments, pass the first one as event data
            auto firstArg = std::get<0>(std::make_tuple(args...));
            if constexpr (std::is_convertible_v<decltype(firstArg), int>) {
                m_Recorder->OnGameEvent(eventName, static_cast<int>(firstArg));
            } else {
                m_Recorder->OnGameEvent(eventName, 0);
            }
        } else {
            m_Recorder->OnGameEvent(eventName, 0);
        }
    }
}

// Explicit template instantiations for events used in BallanceTAS.cpp
template void TASEngine::OnGameEvent(const std::string &);
template void TASEngine::OnGameEvent(const std::string &, int);

bool TASEngine::LoadTAS(const TASProject *project) {
    if (m_ShuttingDown || !project || !project->IsValid()) {
        m_Mod->GetLogger()->Error("Invalid TAS project provided or shutting down.");
        return false;
    }

    UnloadTAS(); // Ensure no other script is running

    try {
        // Load and execute the main script file in the Lua VM
        auto result = m_LuaState.safe_script_file(project->GetEntryScriptPath(), &sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            m_Mod->GetLogger()->Error("Failed to execute script: %s", err.what());
            return false;
        }

        // The script should define a global 'main' function
        sol::function mainFunc = m_LuaState["main"];
        if (!mainFunc.valid()) {
            m_Mod->GetLogger()->Error("'main' function not found in entry script.");
            return false;
        }

        if (m_Scheduler) {
            m_Scheduler->StartCoroutine(mainFunc);
        }

        m_Mod->GetLogger()->Info("TAS script '%s' loaded and started.", project->GetName().c_str());
        return true;
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Exception loading TAS: %s", e.what());
        return false;
    }
}

void TASEngine::UnloadTAS() {
    if (m_Scheduler && m_Scheduler->IsRunning()) {
        m_Scheduler->Clear();
        if (m_InputSystem) {
            m_InputSystem->ReleaseAllKeys();
        }
        m_Mod->GetLogger()->Info("TAS script unloaded and stopped.");
    }
}

void TASEngine::SetDeveloperMode(bool enabled) {
    if (m_ShuttingDown) {
        return;
    }

    if (m_DevTools) {
        m_DevTools->SetEnabled(enabled);
        // Re-register APIs to add/remove the tas.tools table
        try {
            LuaApi::Register(this);
        } catch (const std::exception &e) {
            m_Mod->GetLogger()->Error("Failed to re-register Lua APIs: %s", e.what());
        }
    }
}