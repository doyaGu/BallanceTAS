#include "TASEngine.h"

#include <algorithm>
#include <fstream>

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

TASEngine::TASEngine(BallanceTAS *mod) : m_Mod(mod) {}

TASEngine::~TASEngine() = default;

bool TASEngine::Initialize() {
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
            SetRecording(isRecording);
            m_Mod->SetUIMode(isRecording ? 2 : 0); // 2 = Recording mode
        });
    }

    m_Mod->GetLogger()->Info("TASEngine and all subsystems initialized.");
    return true;
}

void TASEngine::Shutdown() {
    try {
        // Clear callbacks first
        ClearCallbacks();

        // Stop any active recording or playback
        if (IsRecording()) {
            StopRecording();
        }
        if (IsPlaying()) {
            UnloadTAS();
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
    if (m_State & (TAS_PLAYING | TAS_RECORDING)) {
        return; // Already active
    }

    // Check if we should start recording instead of playing
    if (IsPendingRecord()) {
        // Clear any existing callbacks first to prevent duplicates
        ClearCallbacks();

        // Set up callbacks for recording mode
        SetupRecordingCallbacks();

        m_GameInterface->AcquireKeyBindings();

        m_Mod->GetBML()->AddTimer(1ul, [this]() {
            try {
                m_GameInterface->ResetPhysicsTime();
                m_GameInterface->SetCurrentTick(0);

                if (!StartRecording()) {
                    m_Mod->GetLogger()->Error("Failed to start recording.");
                    Stop();
                    return;
                }

                SetRecordPending(false);
                SetRecording(true);

                // Notify UI of state change
                m_Mod->SetUIMode(2); // UIMode::Recording
                m_Mod->GetLogger()->Info("Started recording new TAS.");
            } catch (const std::exception &e) {
                m_Mod->GetLogger()->Error("Exception during recording start: %s", e.what());
                Stop();
            }
        });
        return;
    }

    // Normal TAS playback logic
    TASProject *project = m_ProjectManager->GetCurrentProject();
    if (!project || !project->IsValid()) {
        m_Mod->GetLogger()->Error("No valid TAS project selected.");
        return;
    }

    // Clear any existing callbacks first to prevent duplicates
    ClearCallbacks();

    // Set up callbacks for playback mode
    SetupPlaybackCallbacks();

    m_GameInterface->AcquireKeyBindings();

    m_Mod->GetBML()->AddTimer(1ul, [this, project]() {
        try {
            m_GameInterface->ResetPhysicsTime();
            m_GameInterface->SetCurrentTick(0);

            if (!LoadTAS(project)) {
                m_Mod->GetLogger()->Error("Failed to load TAS project.");
                Stop();
                return;
            }

            SetPlayPending(false);
            SetPlaying(true);

            // Notify UI of state change
            m_Mod->SetUIMode(1); // UIMode::Playing
            m_Mod->GetLogger()->Info("Started playing TAS project: %s", project->GetName().c_str());
        } catch (const std::exception &e) {
            m_Mod->GetLogger()->Error("Exception during TAS start: %s", e.what());
            Stop();
        }
    });
}

void TASEngine::Stop() {
    if (!IsPlaying() && !IsRecording() && !IsPendingPlay() && !IsPendingRecord()) {
        return;
    }

    try {
        if (IsRecording()) {
            StopRecording();
        }

        if (m_Scheduler) {
            m_Scheduler->Clear();
        }

        ClearCallbacks();

        m_Mod->GetBML()->AddTimer(1ul, [this]() {
            try {
                m_GameInterface->SetPhysicsTimeFactor();
                m_GameInterface->SetCurrentTick(0);

                // Notify UI of state change
                m_Mod->SetUIMode(0); // UIMode::Idle
                SetPlaying(false);
                SetRecording(false);
                SetPlayPending(false);
                SetRecordPending(false);
                m_Mod->GetLogger()->Info("TASEngine stopped.");
            } catch (const std::exception &e) {
                m_Mod->GetLogger()->Error("Exception during TAS stop: %s", e.what());
                SetPlaying(false);
                SetRecording(false);
                SetPlayPending(false);
                SetRecordPending(false);
            }
        });
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Exception while stopping TAS: %s", e.what());
        SetPlaying(false);
        SetRecording(false);
        SetPlayPending(false);
        SetRecordPending(false);
    }
}

bool TASEngine::SetupRecording() {
    if (IsRecording() || IsPlaying() || IsPendingPlay()) {
        m_Mod->GetLogger()->Warn("Cannot setup recording: TAS is already active.");
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

bool TASEngine::StartRecording() {
    if (!IsPendingRecord() && !IsRecording()) {
        m_Mod->GetLogger()->Warn("Recording not setup or already active.");
        return false;
    }

    if (!m_Recorder) {
        m_Mod->GetLogger()->Error("Recorder not initialized.");
        return false;
    }

    try {
        // Start the recorder
        m_Recorder->Start();
        m_Mod->GetLogger()->Info("Recording started.");
        return true;
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Exception starting recording: %s", e.what());
        return false;
    }
}

bool TASEngine::StopRecordingAndGenerate(const std::string &projectName, const GenerationOptions *options) {
    if (!IsRecording()) {
        m_Mod->GetLogger()->Warn("Not currently recording.");
        return false;
    }

    if (!m_Recorder || !m_ScriptGenerator) {
        m_Mod->GetLogger()->Error("Recording subsystems not initialized.");
        return false;
    }

    try {
        // Stop recording and get the frame data
        auto frames = m_Recorder->Stop();

        if (frames.empty()) {
            m_Mod->GetLogger()->Warn("No frames recorded, cannot generate script.");
            SetRecording(false);
            return false;
        }

        m_Mod->GetLogger()->Info("Recording stopped. Generating script from %zu frames...", frames.size());

        // Generate the script
        bool success;
        if (options) {
            success = m_ScriptGenerator->Generate(frames, *options);
        } else {
            success = m_ScriptGenerator->Generate(frames, projectName);
        }

        if (success) {
            m_Mod->GetLogger()->Info("Script generated successfully: %s", projectName.c_str());

            // Refresh projects to include the new one
            m_ProjectManager->RefreshProjects();
        } else {
            m_Mod->GetLogger()->Error("Failed to generate script.");
        }

        // Clean up recording state
        ClearCallbacks();
        SetRecording(false);
        m_Mod->SetUIMode(0); // UIMode::Idle

        return success;
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Exception during script generation: %s", e.what());
        SetRecording(false);
        return false;
    }
}

void TASEngine::StopRecording() {
    if (!IsRecording()) {
        return;
    }

    try {
        if (m_Recorder) {
            m_Recorder->Stop();
        }

        ClearCallbacks();
        SetRecording(false);
        m_Mod->SetUIMode(0); // UIMode::Idle

        m_Mod->GetLogger()->Info("Recording stopped without generating script.");
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Exception stopping recording: %s", e.what());
        SetRecording(false);
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
    CKTimeManagerHook::AddPostCallback([this](CKBaseManager *man) {
        try {
            auto *timeManager = static_cast<CKTimeManager *>(man);
            TASProject *project = m_ProjectManager->GetCurrentProject();
            if (project && project->IsValid()) {
                timeManager->SetLastDeltaTime(project->GetDeltaTime());
            }
        } catch (const std::exception &e) {
            m_Mod->GetLogger()->Error("Time callback error: %s", e.what());
        }
    });

    CKInputManagerHook::AddPostCallback([this](CKBaseManager *man) {
        try {
            if (IsPlaying()) {
                m_GameInterface->IncrementCurrentTick();
            }

            m_Scheduler->Tick();

            if (m_Scheduler->IsRunning()) {
                auto *inputManager = static_cast<CKInputManager *>(man);
                m_InputSystem->Apply(inputManager->GetKeyboardState());
            }

        } catch (const std::exception &e) {
            m_Mod->GetLogger()->Error("Input callback error: %s", e.what());
        }
    });
}

void TASEngine::SetupRecordingCallbacks() {
    CKTimeManagerHook::AddPostCallback([this](CKBaseManager *man) {
        try {
            auto *timeManager = static_cast<CKTimeManager *>(man);
            if (IsRecording()) {
                timeManager->SetLastDeltaTime(m_Recorder->GetDeltaTime());
            }
        } catch (const std::exception &e) {
            m_Mod->GetLogger()->Error("Time callback error: %s", e.what());
        }
    });

    CKInputManagerHook::AddPostCallback([this](CKBaseManager *man) {
        try {
            if (IsRecording()) {
                m_GameInterface->IncrementCurrentTick();
            }

            // In recording mode, we capture real input BEFORE any modification
            TickRecording();
        } catch (const std::exception &e) {
            m_Mod->GetLogger()->Error("Recording callback error: %s", e.what());
        }
    });
}

void TASEngine::TickRecording() {
    if (!IsRecording() || !m_Recorder) {
        return;
    }

    // Let the recorder capture this frame's input and events
    m_Recorder->Tick();
}

template <typename... Args>
void TASEngine::OnGameEvent(const std::string &eventName, Args... args) {
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
    if (!project || !project->IsValid()) {
        m_Mod->GetLogger()->Error("Invalid TAS project provided.");
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

        m_Scheduler->StartCoroutine(mainFunc);

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
            m_InputSystem->ReleaseAllKeys(); // Important cleanup step
        }
        m_Mod->GetLogger()->Info("TAS script unloaded and stopped.");
    }
}

void TASEngine::SetDeveloperMode(bool enabled) {
    if (m_DevTools) {
        m_DevTools->SetEnabled(enabled);
        // Re-register APIs to add/remove the tas.tools table
        LuaApi::Register(this);
    }
}
