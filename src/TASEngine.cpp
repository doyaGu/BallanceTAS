#include "TASEngine.h"

#include "Logger.h"
#include "GameInterface.h"
#include "ProjectManager.h"
#include "InputSystem.h"
#include "DX8InputManager.h"
#include "EventManager.h"
#include "TASHook.h"
#include "TASProject.h"
#include "Recorder.h"
#include "ScriptGenerator.h"
#include "ScriptContextManager.h"
#include "ScriptContext.h"
#ifdef ENABLE_REPL
#include "LuaREPLServer.h"
#endif
#include "RecordPlayer.h"
#include "UIManager.h"
#include "StartupProjectManager.h"

#include "TASStateMachine.h"
#include "TASStateHandlers.h"
#include "TASControllers.h"

#include "ServiceContainer.h"

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
        Log::Error("Cannot initialize TASEngine during shutdown.");
        return false;
    }

    // 0. Create ServiceContainer
    try {
        m_ServiceContainer = std::make_unique<ServiceContainer>();
        Log::Info("ServiceContainer initialized.");

        // Register external dependencies (TASEngine retains ownership)
        m_ServiceContainer->RegisterSingletonPtr(m_GameInterface);
        m_ServiceContainer->RegisterSingletonPtr(this); // Register TASEngine itself for Strategy creation
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize ServiceContainer: %s", e.what());
        return false;
    }

    // 1. Create Core Subsystems and transfer ownership to ServiceContainer
    try {
        // Create subsystems
        auto inputSystem = std::make_unique<InputSystem>();
        auto eventManager = std::make_unique<EventManager>();
        auto recorder = std::make_unique<Recorder>(this);
        auto scriptGenerator = std::make_unique<ScriptGenerator>(this);
        auto scriptContextManager = std::make_unique<ScriptContextManager>(this);
        auto recordPlayer = std::make_unique<RecordPlayer>(this);
        auto startupProjectManager = std::make_unique<StartupProjectManager>(this);

        // Transfer ownership to container
        m_ServiceContainer->RegisterSingletonInstance(std::move(inputSystem));
        m_ServiceContainer->RegisterSingletonInstance(std::move(eventManager));
        m_ServiceContainer->RegisterSingletonInstance(std::move(recorder));
        m_ServiceContainer->RegisterSingletonInstance(std::move(scriptGenerator));
        m_ServiceContainer->RegisterSingletonInstance(std::move(scriptContextManager));
        m_ServiceContainer->RegisterSingletonInstance(std::move(recordPlayer));
        m_ServiceContainer->RegisterSingletonInstance(std::move(startupProjectManager));

        // Initialize state machine
        auto stateMachine = std::make_unique<TASStateMachine>(this);

        // Register state handlers
        stateMachine->RegisterHandler(TASStateMachine::State::Idle,
                                      std::make_unique<IdleHandler>(this));
        stateMachine->RegisterHandler(TASStateMachine::State::Recording,
                                      std::make_unique<RecordingHandler>(this));
        stateMachine->RegisterHandler(TASStateMachine::State::PlayingScript,
                                      std::make_unique<PlayingScriptHandler>(this));
        stateMachine->RegisterHandler(TASStateMachine::State::PlayingRecord,
                                      std::make_unique<PlayingRecordHandler>(this));
        stateMachine->RegisterHandler(TASStateMachine::State::Translating,
                                      std::make_unique<TranslatingHandler>(this));
        stateMachine->RegisterHandler(TASStateMachine::State::Paused,
                                      std::make_unique<PausedHandler>(this));

        Log::Info("State machine initialized with all handlers registered.");

        // Transfer to container
        m_ServiceContainer->RegisterSingletonInstance(std::move(stateMachine));

        // Initialize controllers
        auto provider = GetServiceProvider();

        auto recordingController = std::make_unique<RecordingController>(provider);
        auto playbackController = std::make_unique<PlaybackController>(provider);
        auto translationController = std::make_unique<TranslationController>(provider);

        // Transfer to container
        m_ServiceContainer->RegisterSingletonInstance(std::move(recordingController));
        m_ServiceContainer->RegisterSingletonInstance(std::move(playbackController));
        m_ServiceContainer->RegisterSingletonInstance(std::move(translationController));

        // Initialize controllers (they might fail, but that's okay - they'll log errors)
        auto recordingCtrl = m_ServiceContainer->Resolve<RecordingController>();
        auto playbackCtrl = m_ServiceContainer->Resolve<PlaybackController>();
        auto translationCtrl = m_ServiceContainer->Resolve<TranslationController>();

        if (recordingCtrl) recordingCtrl->Initialize();
        if (playbackCtrl) playbackCtrl->Initialize();
        if (translationCtrl) translationCtrl->Initialize();

        Log::Info("Controllers initialized.");
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize core subsystems: %s", e.what());
        return false;
    }

    // 2. Initialize execution subsystems
    try {
        // Initialize ScriptContextManager (multi-context system)
        if (!m_ScriptContextManager->Initialize()) {
            Log::Error("Failed to initialize ScriptContextManager.");
            return false;
        }

#ifdef ENABLE_REPL
        // Initialize REPL server (optional - for remote debugging)
        auto replServer = std::make_unique<LuaREPLServer>(this);
        if (replServer->Initialize(7878, "")) {
            // Default port, no auth
            if (replServer->Start()) {
                Log::Info("REPL server started on port 7878");
                // Transfer to container only if successful
                m_ServiceContainer->RegisterSingletonInstance(std::move(replServer));
            } else {
                Log::Warn("Failed to start REPL server, not registering in container");
            }
        } else {
            Log::Warn("Failed to initialize REPL server, not registering in container");
        }
#endif // ENABLE_REPL
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize execution subsystems: %s", e.what());
        return false;
    }

    // 3. Initialize Project Manager
    try {
        auto projectManager = std::make_unique<ProjectManager>(this);
        m_ServiceContainer->RegisterSingletonInstance(std::move(projectManager));
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize project manager: %s", e.what());
        return false;
    }

    // 4. Initialize StartupProjectManager
    try {
        auto startupMgr = GetStartupProjectManager();
        if (startupMgr && !startupMgr->Initialize()) {
            Log::Error("Failed to initialize StartupProjectManager.");
            return false;
        }
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize startup project manager: %s", e.what());
        return false;
    }

    // 5. Set up callbacks
    auto recordPlayer = GetRecordPlayer();
    if (recordPlayer) {
        recordPlayer->SetStatusCallback([this](bool isPlaying) {
            if (m_ShuttingDown) return;

            if (!isPlaying && IsPlaying() && m_PlaybackType == PlaybackType::Record) {
                StopReplay();
            }
        });
    }

    auto recorder = GetRecorder();
    if (recorder) {
        recorder->SetStatusCallback([this](bool isRecording) {
            if (m_ShuttingDown) return;

            m_GameInterface->SetUIMode(isRecording ? UIMode::Recording : UIMode::Idle);
        });
    }

    Log::Info("TASEngine and all subsystems initialized.");
    Log::Info("ServiceContainer holds %zu registered services.", m_ServiceContainer->GetServiceCount());
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

        if (IsPlaying()) {
            StopReplayImmediate();
        }

        // Stop any active recording or playback immediately (without timers)
        if (IsRecording()) {
            StopRecordingImmediate();
        }

        // Call shutdown methods on subsystems
#ifdef ENABLE_REPL
        auto replServer = GetREPLServer();
        if (replServer) {
            replServer->Shutdown();
        }
#endif

        auto scriptCtxMgr = GetScriptContextManager();
        if (scriptCtxMgr) {
            scriptCtxMgr->Shutdown();
        }

        // Destroy ServiceProvider first (it references the container)
        m_ServiceProvider.reset();

        // Clear ServiceContainer - this will destroy all registered services
        m_ServiceContainer->Clear();

        Log::Info("TASEngine shutdown complete. ServiceContainer cleared.");
    } catch (const std::exception &e) {
        Log::Error("Exception during TASEngine shutdown: %s", e.what());
    }

    m_GameInterface = nullptr;
}

void TASEngine::Start() {
    if (m_ShuttingDown || IsPlaying() || IsRecording() || IsTranslating()) {
        return; // Already active or shutting down
    }

    AddTimer(1ul, [this]() {
        if (m_GameInterface) {
            m_GameInterface->ResetPhysicsTime();
        }
    });

    // Check for translation mode first
    if (IsPendingTranslate()) {
        StartTranslationInternal();
        return;
    }

    // Check if we should start playing
    if (IsPendingPlay()) {
        StartReplayInternal();
        return;
    }

    // Check if we should start recording instead of playing
    if (IsPendingRecord()) {
        StartRecordingInternal();
        return;
    }
}

void TASEngine::Stop() {
    if (m_ShuttingDown) {
        return;
    }

    // Check if we should keep global scripts running during level transitions
    bool shouldKeepScript = false;
    if (IsPlaying() && m_PlaybackType == PlaybackType::Script && m_ScriptContextManager) {
        // Global contexts persist across level transitions
        auto globalCtx = m_ScriptContextManager->GetContext("global");
        if (globalCtx && globalCtx->IsExecuting() && globalCtx->GetCurrentProject()) {
            if (globalCtx->GetCurrentProject()->IsGlobalProject()) {
                shouldKeepScript = true;
                Log::Info("Keeping global script running during level transition");
            }
        }
    }

    if (IsTranslating()) {
        StopTranslation();
    } else if (IsPlaying()) {
        StopReplay(!shouldKeepScript); // Clear project only if not global script
    } else if (IsRecording()) {
        StopRecording();
    } else {
        // Stop any pending operations
        m_PendingOperation = PendingOperation::None;
        m_PlaybackType = PlaybackType::None;
        m_GameInterface->SetUIMode(UIMode::Idle);
    }

    if (IsAutoRestartEnabled()) {
        RestartCurrentProject();
    }
}

// === Recording Control ===

bool TASEngine::StartRecording() {
    if (m_ShuttingDown || IsRecording() || IsPlaying() || IsPendingPlay()) {
        Log::Warn("Cannot start recording: TAS is already active or shutting down.");
        return false;
    }

    if (!m_RecordingController) {
        Log::Error("RecordingController not initialized.");
        return false;
    }

    // Set pending - actual recording will start in Start() when level loads
    SetRecordPending(true);
    Log::Info("Recording setup complete. Will start when level loads.");
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

    if (!m_RecordingController) {
        Log::Error("RecordingController not initialized.");
        return;
    }

    try {
        if (IsRecording()) {
            auto result = m_RecordingController->StopRecording(false);
            if (!result.IsOk()) {
                Log::Error("Failed to stop recording: %s", result.GetError().message.c_str());
            } else {
                // Handle recorded frames if needed
                auto &frames = result.Unwrap();
                Log::Info("Recording stopped, captured %zu frames", frames.size());

                // Validation dump (if enabled)
                if (IsValidationEnabled() && m_Recorder) {
                    std::string path = m_Path;
                    path.append("\\").append(m_Recorder->GetGenerationOptions().projectName)
                        .append("\\recording_").append(std::to_string(std::time(nullptr))).append(".txt");
                    if (!m_Recorder->DumpFrameData(path, true)) {
                        Log::Error("Failed to dump frame data to: %s", path.c_str());
                    }
                }
            }
        }
    } catch (const std::exception &e) {
        Log::Error("Exception stopping recording: %s", e.what());
    }

    ClearCallbacks();
    SetRecording(false);
    SetRecordPending(false);
}

void TASEngine::StopRecordingImmediate() {
    try {
        if (IsRecording() && m_RecordingController) {
            // Disable auto-generation before stopping
            if (m_Recorder) {
                m_Recorder->SetAutoGenerate(false);
            }

            m_RecordingController->StopRecording(true); // immediate = true
        }

        SetRecording(false);
        SetRecordPending(false);

        m_GameInterface->SetUIMode(UIMode::Idle);
        Log::Info("Recording stopped immediately.");
    } catch (const std::exception &e) {
        Log::Error("Exception during immediate recording stop: %s", e.what());
    }
}

size_t TASEngine::GetRecordingFrameCount() const {
    if (!IsRecording() || !m_Recorder) {
        return 0;
    }
    return m_Recorder->GetTotalFrames();
}

// === Replay Control ===

bool TASEngine::StartReplay() {
    if (m_ShuttingDown || IsPlaying() || IsRecording() || IsPendingRecord()) {
        Log::Warn("Cannot start replay: TAS is already active or shutting down.");
        return false;
    }

    if (!m_PlaybackController) {
        Log::Error("PlaybackController not initialized.");
        return false;
    }

    TASProject *project = m_ProjectManager->GetCurrentProject();
    if (!project || !project->IsValid()) {
        Log::Error("No valid TAS project selected.");
        return false;
    }

    // Set pending - actual playback will start in StartReplayInternal when level loads
    SetPlayPending(true);
    Log::Info("Replay setup complete. Will start when level loads.");
    return true;
}

void TASEngine::StopReplay(bool clearProject) {
    if (m_ShuttingDown) {
        StopReplayImmediate();
        return;
    }

    if (!IsPlaying() && !IsPendingPlay()) {
        return;
    }

    if (!m_PlaybackController) {
        Log::Error("PlaybackController not initialized.");
        return;
    }

    try {
        // Stop playback via controller
        m_PlaybackController->StopPlayback(false); // Don't clear project in controller
    } catch (const std::exception &e) {
        Log::Error("Exception stopping replay: %s", e.what());
    }

    // Clean up validation recording if active
    if (IsValidationEnabled()) {
        Log::Info("Stopping validation recording due to playback end.");
        StopValidationRecording();
    }

    // Reset keyboard state to ensure clean state
    memset(m_GameInterface->GetInputManager()->GetKeyboardState(), KS_IDLE, 256);

    // Only clear project if explicitly requested
    if (clearProject) {
        m_ProjectManager->SetCurrentProject(nullptr);
    }

    ClearCallbacks();
    SetPlaying(false);
    SetPlayPending(false);
    m_PlaybackType = PlaybackType::None;

    m_GameInterface->SetUIMode(UIMode::Idle);
    Log::Info("Replay stopped.");
}

void TASEngine::StopReplayImmediate() {
    try {
        // Stop playback via controller
        if (m_PlaybackController) {
            m_PlaybackController->StopPlayback(true); // Clear project
        }

        if (IsValidationEnabled()) {
            Log::Info("Stopping validation recording due to playback end.");
            StopValidationRecording();
        }

        // Reset keyboard state to ensure clean state
        memset(m_GameInterface->GetInputManager()->GetKeyboardState(), KS_IDLE, 256);

        ClearCallbacks();

        SetPlaying(false);
        SetPlayPending(false);
        m_PlaybackType = PlaybackType::None;

        m_GameInterface->SetUIMode(UIMode::Idle);
        Log::Info("Replay stopped immediately.");
    } catch (const std::exception &e) {
        Log::Error("Exception during immediate replay stop: %s", e.what());
    }
}

// === Translation Control ===

bool TASEngine::StartTranslation() {
    if (m_ShuttingDown || IsTranslating() || IsPlaying() || IsRecording() ||
        IsPendingPlay() || IsPendingRecord()) {
        Log::Warn("Cannot start translation: TAS is already active or shutting down.");
        return false;
    }

    if (!m_TranslationController) {
        Log::Error("TranslationController not initialized.");
        return false;
    }

    TASProject *project = m_ProjectManager->GetCurrentProject();
    if (!project || !project->IsRecordProject() || !project->IsValid()) {
        Log::Error("Translation requires a valid record project (.tas file).");
        return false;
    }

    // Check if record can be accurately translated
    if (!project->CanBeTranslated()) {
        Log::Error("Record cannot be accurately translated: %s",
                   project->GetTranslationCompatibilityMessage().c_str());
        return false;
    }

    // Set pending - actual translation will start in StartTranslationInternal when level loads
    SetTranslatePending(true);
    Log::Info("Translation setup complete. Will start when level loads.");
    Log::Info("Translating record: %s", project->GetName().c_str());
    return true;
}

void TASEngine::StopTranslation(bool clearProject) {
    if (m_ShuttingDown) {
        StopTranslationImmediate();
        return;
    }

    if (!IsTranslating() && !IsPendingTranslate()) {
        return;
    }

    if (!m_TranslationController) {
        Log::Error("TranslationController not initialized.");
        return;
    }

    try {
        // Stop translation via controller
        m_TranslationController->StopTranslation(false); // Don't clear project in controller
    } catch (const std::exception &e) {
        Log::Error("Exception stopping translation: %s", e.what());
    }

    // Reset keyboard state to ensure clean state
    memset(m_GameInterface->GetInputManager()->GetKeyboardState(), KS_IDLE, 256);

    // Only clear project if explicitly requested
    if (clearProject) {
        m_ProjectManager->SetCurrentProject(nullptr);
    }

    ClearCallbacks();
    SetTranslating(false);
    SetTranslatePending(false);

    m_GameInterface->SetUIMode(UIMode::Idle);
    Log::Info("Translation completed and script generated.");
}

void TASEngine::StopTranslationImmediate() {
    try {
        // Stop translation via controller
        if (m_TranslationController) {
            m_TranslationController->StopTranslation(true); // Clear project
        }

        // Reset keyboard state
        memset(m_GameInterface->GetInputManager()->GetKeyboardState(), KS_IDLE, 256);

        SetTranslating(false);
        SetTranslatePending(false);

        m_GameInterface->SetUIMode(UIMode::Idle);
        Log::Info("Translation stopped immediately.");
    } catch (const std::exception &e) {
        Log::Error("Exception during immediate translation stop: %s", e.what());
    }
}

bool TASEngine::StartValidationRecording(const std::string &outputPath) {
    if (!IsPlayingScript()) {
        Log::Error("Validation recording can only be enabled during script playback.");
        return false;
    }

    if (!m_Recorder) {
        Log::Error("Recorder subsystem not available for validation recording.");
        return false;
    }

    if (m_Recorder->IsRecording()) {
        Log::Error("Cannot enable validation recording while regular recording is active.");
        return false;
    }

    m_ValidationOutputPath = outputPath;
    m_ValidationRecording = true;

    // Start validation recording
    m_Recorder->SetAutoGenerate(false);
    m_Recorder->ClearFrameData();
    m_Recorder->Start();

    Log::Info("Validation recording enabled - output path: %s", outputPath.c_str());
    return true;
}

bool TASEngine::StopValidationRecording() {
    if (!m_ValidationRecording) {
        Log::Warn("Validation recording is not currently enabled.");
        return false;
    }

    if (!m_Recorder || !m_Recorder->IsRecording()) {
        Log::Error("Validation recording state inconsistent - recorder not active.");
        m_ValidationRecording = false;
        return false;
    }

    // Stop recording and get frame data
    auto frameData = m_Recorder->Stop();

    // Generate validation dumps with timestamped filename
    std::string timestampedPath = m_ValidationOutputPath + "validation_" +
        std::to_string(std::time(nullptr)) + ".txt";

    bool success = m_Recorder->DumpFrameData(timestampedPath, true);
    if (success) {
        Log::Info("Validation recording completed - %zu frames captured, dumps saved to: %s",
                  frameData.size(), timestampedPath.c_str());
    } else {
        Log::Error("Failed to generate validation dumps to: %s", timestampedPath.c_str());
    }

    m_ValidationRecording = false;
    m_ValidationOutputPath.clear();
    return success;
}

bool TASEngine::RestartCurrentProject() {
    if (m_ShuttingDown) {
        Log::Warn("Cannot restart during shutdown.");
        return false;
    }

    TASProject *project = m_ProjectManager->GetCurrentProject();
    if (!project || !project->IsValid()) {
        Log::Error("No valid project to restart.");
        return false;
    }

    Log::Info("Restarting TAS project: %s", project->GetName().c_str());

    // Stop current execution without clearing project
    if (IsPlaying()) {
        StopReplay(false);
    }
    if (IsTranslating()) {
        StopTranslation(false);
    }

    if (m_ProjectManager->GetCurrentProject()) {
        StartReplay();
    }

    return true;
}

// === Internal Start Methods ===

void TASEngine::StartRecordingInternal() {
    if (m_ShuttingDown) {
        return;
    }

    if (!m_RecordingController) {
        Log::Error("RecordingController not initialized.");
        return;
    }

    // Clear any existing callbacks first to prevent duplicates
    ClearCallbacks();

    // Set up callbacks for recording mode
    SetupRecordingCallbacks();

    SetCurrentTick(0);

    m_GameInterface->AcquireKeyBindings();

    try {
        // Start recording via controller (validation mode if enabled)
        auto result = m_RecordingController->StartRecording(IsValidationEnabled());
        if (!result.IsOk()) {
            Log::Error("Failed to start recording: %s", result.GetError().message.c_str());
            Stop();
            return;
        }
    } catch (const std::exception &e) {
        Log::Error("Exception during recording start: %s", e.what());
        Stop();
        return;
    }

    SetRecordPending(false);
    SetRecording(true);

    Log::Info("Started recording new TAS.");
}

void TASEngine::StartReplayInternal() {
    if (m_ShuttingDown) {
        return;
    }

    if (!m_PlaybackController) {
        Log::Error("PlaybackController not initialized.");
        Stop();
        return;
    }

    TASProject *project = m_ProjectManager->GetCurrentProject();
    if (!project || !project->IsValid()) {
        Log::Error("No valid TAS project selected.");
        Stop();
        return;
    }

    // Determine playback type
    PlaybackType playbackType = DeterminePlaybackType(project);
    if (playbackType == PlaybackType::None) {
        Log::Error("Unable to determine playback type for project: %s", project->GetName().c_str());
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

    m_GameInterface->AcquireKeyBindings();

    try {
        // Start playback via controller
        auto result = m_PlaybackController->StartPlayback(project, playbackType);
        if (!result.IsOk()) {
            Log::Error("Failed to start playback: %s", result.GetError().message.c_str());
            Stop();
            return;
        }
    } catch (const std::exception &e) {
        Log::Error("Exception during playback start: %s", e.what());
        Stop();
        return;
    }

    SetPlayPending(false);
    m_PlaybackType = playbackType; // Set PlaybackType BEFORE SetPlaying for StateMachine
    SetPlaying(true);

    if (IsValidationEnabled()) {
        std::string path = project->GetPath();
        path.append("\\");
        StartValidationRecording(path);
    }

    m_GameInterface->SetUIMode(UIMode::Playing);
    Log::Info("Started playing TAS project: %s (%s mode)",
              project->GetName().c_str(),
              playbackType == PlaybackType::Script ? "Script" : "Record");
}

void TASEngine::StartTranslationInternal() {
    if (m_ShuttingDown) {
        return;
    }

    if (!m_TranslationController) {
        Log::Error("TranslationController not initialized.");
        Stop();
        return;
    }

    TASProject *project = m_ProjectManager->GetCurrentProject();
    if (!project || !project->IsRecordProject() || !project->IsValid()) {
        Log::Error("No valid record project selected for translation.");
        Stop();
        return;
    }

    // Clear any existing callbacks first to prevent duplicates
    ClearCallbacks();

    // Set up translation callbacks (combines recording and playback)
    SetupTranslationCallbacks();

    SetCurrentTick(0);

    m_GameInterface->AcquireKeyBindings();

    // For translation, InputSystem should be DISABLED
    // We want RecordPlayer to control input directly, and Recorder to capture it
    if (m_InputSystem) {
        m_InputSystem->Reset();
        m_InputSystem->SetEnabled(false);
    }

    try {
        // Set up generation options for translation
        GenerationOptions options;
        options.projectName = project->GetName() + "_Script";
        options.authorName = project->GetAuthor();
        options.targetLevel = project->GetTargetLevel();
        options.description = "Translated from legacy record: " + project->GetName();
        options.updateRate = project->GetUpdateRate();
        options.addFrameComments = true;

        // Start translation via controller
        auto result = m_TranslationController->StartTranslation(project, options);
        if (!result.IsOk()) {
            Log::Error("Failed to start translation: %s", result.GetError().message.c_str());
            Stop();
            return;
        }
    } catch (const std::exception &e) {
        Log::Error("Exception during translation start: %s", e.what());
        Stop();
        return;
    }

    SetTranslatePending(false);
    SetTranslating(true);

    m_GameInterface->SetUIMode(UIMode::Recording); // Show as recording since we're generating a script
    Log::Info("Started translation of record: %s", project->GetName().c_str());
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

                    IncrementCurrentTick();
                }
            } catch (const std::exception &e) {
                Log::Error("Recording callback error: %s", e.what());
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
#ifdef ENABLE_REPL
                // STEP 0: REPL server tick start (process scheduled commands)
                if (m_REPLServer &&m_REPLServer->IsRunning()) {
                    m_REPLServer->OnTickStart(m_CurrentTick);
                    m_REPLServer->ProcessImmediateCommands();
                }
#endif

                // STEP 1: Tick all script contexts (multi-context system)
                if (m_ScriptContextManager) {
                    m_ScriptContextManager->TickAll();
                }

                // STEP 2: Apply merged inputs from all contexts
                auto *inputManager = static_cast<DX8InputManager *>(man);
                ApplyMergedContextInputs(inputManager);

                // STEP 3: Validation recording
                if (m_ValidationRecording && m_Recorder && m_Recorder->IsRecording()) {
                    auto *inputManager = static_cast<CKInputManager *>(man);
                    m_Recorder->Tick(m_CurrentTick, inputManager->GetKeyboardState());
                }

                // STEP 4: Increment frame counter for next iteration
                IncrementCurrentTick();

#ifdef ENABLE_REPL
                // STEP 5: REPL server tick end (send notifications)
                if (m_REPLServer &&m_REPLServer->IsRunning()) {
                    m_REPLServer->OnTickEnd(m_CurrentTick);
                }
#endif
            } catch (const std::exception &e) {
                Log::Error("Script playback callback error: %s", e.what());
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

                IncrementCurrentTick();
            } catch (const std::exception &e) {
                Log::Error("Record playback callback error: %s", e.what());
                // Stop on error
                if (m_RecordPlayer) {
                    m_RecordPlayer->Stop();
                }
            }
        }
    });
}

void TASEngine::SetupTranslationCallbacks() {
    if (m_ShuttingDown) {
        return;
    }

    // TimeManager callback: Use delta time from record data
    CKTimeManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!m_ShuttingDown && IsTranslating()) {
            auto *timeManager = static_cast<CKTimeManager *>(man);
            TASProject *project = m_ProjectManager->GetCurrentProject();
            if (project && project->IsValid()) {
                timeManager->SetLastDeltaTime(project->GetDeltaTime());
            }
        }
    });

    // InputManager callback: Apply record input and capture it for recording
    CKInputManagerHook::AddPostCallback([this](CKBaseManager *man) {
        if (!m_ShuttingDown && IsTranslating()) {
            try {
                auto *inputManager = static_cast<CKInputManager *>(man);
                unsigned char *keyboardState = inputManager->GetKeyboardState();

                // STEP 1: Apply input from record player
                if (m_RecordPlayer && m_RecordPlayer->IsPlaying()) {
                    m_RecordPlayer->Tick(m_CurrentTick, keyboardState);
                }

                // STEP 2: Capture the applied input with recorder
                if (m_Recorder && m_Recorder->IsRecording()) {
                    m_Recorder->Tick(m_CurrentTick, keyboardState);
                }

                // STEP 3: Increment the current tick for next frame
                IncrementCurrentTick();

                // STEP 4: Check if record playback has finished
                if (m_RecordPlayer && !m_RecordPlayer->IsPlaying() && IsTranslating()) {
                    // Record playback completed, finish translation
                    OnTranslationPlaybackComplete();
                }
            } catch (const std::exception &e) {
                Log::Error("Translation callback error: %s", e.what());
                StopTranslation();
            }
        }
    });
}

void TASEngine::OnTranslationPlaybackComplete() {
    Log::Info("Record playback completed during translation. Generating script...");

    if (IsTranslating()) {
        StopTranslation();
    }
}

// ============================================================================
// Context Lifecycle Management
// ============================================================================

void TASEngine::HandleContextLifecycleEvent(const std::string &eventName) {
    if (!m_ScriptContextManager) {
        return;
    }

    // === Game Start Events ===
    if (eventName == "post_start_menu") {
        // Create global context when game starts
        Log::Info("Creating global context...");
        auto globalContext = m_ScriptContextManager->GetOrCreateGlobalContext();
        if (globalContext) {
            Log::Info("Global context created successfully.");
        } else {
            Log::Error("Failed to create global context.");
        }
    }

    // === Level Start Events ===
    else if (eventName == "start_level") {
        // Create level context when level starts
        std::string levelName = GetCurrentLevelName();
        if (!levelName.empty()) {
            Log::Info("Creating level context for level: %s", levelName.c_str());
            auto levelContext = m_ScriptContextManager->GetOrCreateLevelContext(levelName);
            if (levelContext) {
                Log::Info("Level context created successfully.");

                // Subscribe level context to level-specific events
                m_ScriptContextManager->SubscribeToEvent(levelContext->GetName(), "start_level");
                m_ScriptContextManager->SubscribeToEvent(levelContext->GetName(), "level_finish");
                m_ScriptContextManager->SubscribeToEvent(levelContext->GetName(), "game_over");
                m_ScriptContextManager->SubscribeToEvent(levelContext->GetName(), "pre_checkpoint_reached");
                m_ScriptContextManager->SubscribeToEvent(levelContext->GetName(), "post_checkpoint_reached");
            } else {
                Log::Error("Failed to create level context.");
            }
        }
    }

    // === Level End Events ===
    else if (eventName == "post_exit_level") {
        // Destroy level contexts when leaving level
        Log::Info("Destroying level contexts...");
        m_ScriptContextManager->DestroyAllLevelContexts();
        Log::Info("Level contexts destroyed.");
    }

    // === Game Over / Cleanup Events ===
    else if (eventName == "game_over") {
        // Keep global context but cleanup level contexts
        Log::Info("Game over: cleaning up level contexts...");
        m_ScriptContextManager->DestroyAllLevelContexts();
    }
}

std::string TASEngine::GetCurrentLevelName() const {
    if (!m_GameInterface) {
        return "";
    }

    // Get current level name from GameInterface
    // This assumes GameInterface has a method to get the current level
    // Adjust based on actual GameInterface API
    int currentLevel = m_GameInterface->GetCurrentLevel();
    if (currentLevel > 0) {
        return "Level_" + std::to_string(currentLevel);
    }

    return "";
}

// ============================================================================
// Multi-Context Input Merging
// ============================================================================

void TASEngine::ApplyMergedContextInputs(DX8InputManager *inputManager) {
    if (!inputManager || !m_ScriptContextManager) {
        return;
    }

    // Get all active contexts sorted by priority (highest first)
    auto contexts = m_ScriptContextManager->GetContextsByPriority();

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
    // We apply each InputSystem's state in order, with later ones overriding earlier ones

    // Apply inputs in reverse order (lowest priority first)
    for (auto it = activeInputs.rbegin(); it != activeInputs.rend(); ++it) {
        const InputSystem *inputSys = *it;
        size_t idx = std::distance(it, activeInputs.rend()) - 1;

        // Apply this context's input
        // Note: const_cast is safe here because Apply is conceptually const
        // (it reads the InputSystem state and applies it to the input manager)
        const_cast<InputSystem *>(inputSys)->Apply(m_CurrentTick, inputManager);

        // Log conflicts if this is not the highest priority context
        if (idx < activeInputs.size() - 1) {
            // Check for conflicts with higher priority contexts
            for (size_t j = idx + 1; j < activeInputs.size(); ++j) {
                std::vector<std::string> conflicts;
                if (inputSys->HasConflicts(*activeInputs[j], &conflicts)) {
                    Log::Info("Input conflicts between '%s' and '%s': %zu conflicts",
                              contextNames[idx].c_str(),
                              contextNames[j].c_str(),
                              conflicts.size());
                    // Optionally log detailed conflicts
                    // for (const auto &conflict : conflicts) {
                    //     Log::Info("  - %s", conflict.c_str());
                    // }
                }
            }
        }
    }

    Log::Info("Applied merged inputs from %zu context(s)", activeInputs.size());
}

// ============================================================================
// Game Event Dispatching
// ============================================================================

template <typename... Args>
void TASEngine::OnGameEvent(const std::string &eventName, Args... args) {
    if (m_ShuttingDown) {
        return;
    }

    // === Context Lifecycle Management ===
    HandleContextLifecycleEvent(eventName);

    // === Forward to Multi-Context System ===
    if (m_ScriptContextManager) {
        m_ScriptContextManager->FireGameEventToAll(eventName, args...);
    }

    // === Forward to Recorder ===
    if ((IsRecording() || IsTranslating()) && m_Recorder) {
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

void TASEngine::AddTimer(size_t tick, const std::function<void()> &callback) {
    m_GameInterface->AddTimer(tick, callback);
}

// === Lua State Access ===
// These methods delegate to the global context

sol::state &TASEngine::GetLuaState() {
    if (!m_ScriptContextManager) {
        throw std::runtime_error("ScriptContextManager not initialized");
    }

    // Get or create the global context as the primary context
    auto ctx = m_ScriptContextManager->GetOrCreateGlobalContext();
    if (!ctx) {
        throw std::runtime_error("Failed to get primary context");
    }

    return ctx->GetLuaState();
}

sol::state &TASEngine::GetLuaState() const {
    if (!m_ScriptContextManager) {
        throw std::runtime_error("ScriptContextManager not initialized");
    }

    // Get the global context (const version - don't create)
    auto ctx = m_ScriptContextManager->GetContext("global");
    if (!ctx) {
        throw std::runtime_error("Primary context not available");
    }

    return ctx->GetLuaState();
}

LuaScheduler *TASEngine::GetScheduler() const {
    if (!m_ScriptContextManager) {
        return nullptr;
    }

    // Get the global context
    auto ctx = m_ScriptContextManager->GetContext("global");
    if (!ctx) {
        return nullptr;
    }

    return ctx->GetScheduler();
}

// ============================================================================
// State Query Methods (Using StateMachine)
// ============================================================================

bool TASEngine::IsPlaying() const {
    return m_StateMachine && m_StateMachine->IsPlaying();
}

bool TASEngine::IsRecording() const {
    return m_StateMachine && m_StateMachine->IsRecording();
}

bool TASEngine::IsTranslating() const {
    return m_StateMachine && m_StateMachine->IsTranslating();
}

bool TASEngine::IsIdle() const {
    return m_StateMachine && m_StateMachine->IsIdle();
}

bool TASEngine::IsPaused() const {
    return m_StateMachine && m_StateMachine->IsPaused();
}

bool TASEngine::IsPlayingScript() const {
    return m_StateMachine &&
        m_StateMachine->GetCurrentState() == TASStateMachine::State::PlayingScript;
}

bool TASEngine::IsPlayingRecord() const {
    return m_StateMachine &&
        m_StateMachine->GetCurrentState() == TASStateMachine::State::PlayingRecord;
}

// ============================================================================
// State Setter Methods (Using both legacy bit-mask and StateMachine)
// ============================================================================

void TASEngine::SetPlayPending(bool pending) {
    if (pending) {
        m_PendingOperation = PendingOperation::StartPlaying;
    } else {
        if (m_PendingOperation == PendingOperation::StartPlaying) {
            m_PendingOperation = PendingOperation::None;
        }
    }
}

void TASEngine::SetPlaying(bool playing) {
    if (playing) {
        // Update StateMachine based on PlaybackType
        // Note: PlaybackType should be set BEFORE calling SetPlaying(true)
        if (m_StateMachine) {
            TASStateMachine::State targetState =
                (m_PlaybackType == PlaybackType::Script)
                    ? TASStateMachine::State::PlayingScript
                    : TASStateMachine::State::PlayingRecord;
            auto result = m_StateMachine->ForceSetState(targetState);
            if (!result.IsOk()) {
                Log::Error("Failed to transition StateMachine to playing state: %s",
                           result.GetError().message.c_str());
            }
        }
    } else {
        m_PlaybackType = PlaybackType::None;

        // Transition back to Idle
        if (m_StateMachine) {
            auto result = m_StateMachine->ForceSetState(TASStateMachine::State::Idle);
            if (!result.IsOk()) {
                Log::Error("Failed to transition StateMachine to idle state: %s",
                           result.GetError().message.c_str());
            }
        }
    }
}

void TASEngine::SetRecordPending(bool pending) {
    if (pending) {
        m_PendingOperation = PendingOperation::StartRecording;
    } else {
        if (m_PendingOperation == PendingOperation::StartRecording) {
            m_PendingOperation = PendingOperation::None;
        }
    }
}

void TASEngine::SetRecording(bool recording) {
    if (recording) {
        // Update StateMachine
        if (m_StateMachine) {
            auto result = m_StateMachine->ForceSetState(TASStateMachine::State::Recording);
            if (!result.IsOk()) {
                Log::Error("Failed to transition StateMachine to recording state: %s",
                           result.GetError().message.c_str());
            }
        }
    } else {
        // Transition back to Idle
        if (m_StateMachine) {
            auto result = m_StateMachine->ForceSetState(TASStateMachine::State::Idle);
            if (!result.IsOk()) {
                Log::Error("Failed to transition StateMachine to idle state: %s",
                           result.GetError().message.c_str());
            }
        }
    }
}

void TASEngine::SetTranslatePending(bool pending) {
    if (pending) {
        m_PendingOperation = PendingOperation::StartTranslation;
    } else {
        if (m_PendingOperation == PendingOperation::StartTranslation) {
            m_PendingOperation = PendingOperation::None;
        }
    }
}

void TASEngine::SetTranslating(bool translating) {
    if (translating) {
        // Update StateMachine
        if (m_StateMachine) {
            auto result = m_StateMachine->ForceSetState(TASStateMachine::State::Translating);
            if (!result.IsOk()) {
                Log::Error("Failed to transition StateMachine to translating state: %s",
                           result.GetError().message.c_str());
            }
        }
    } else {
        // Transition back to Idle
        if (m_StateMachine) {
            auto result = m_StateMachine->ForceSetState(TASStateMachine::State::Idle);
            if (!result.IsOk()) {
                Log::Error("Failed to transition StateMachine to idle state: %s",
                           result.GetError().message.c_str());
            }
        }
    }
}

ServiceProvider *TASEngine::GetServiceProvider() const {
    if (!m_ServiceProvider && m_ServiceContainer) {
        // Lazy-initialize ServiceProvider
        m_ServiceProvider = std::make_unique<ServiceProvider>(*m_ServiceContainer);
    }
    return m_ServiceProvider.get();
}

ProjectManager *TASEngine::GetProjectManager() const {
    return m_ProjectManager;
}

InputSystem *TASEngine::GetInputSystem() const {
    return m_InputSystem;
}

EventManager *TASEngine::GetEventManager() const {
    return m_EventManager;
}

ScriptContextManager *TASEngine::GetScriptContextManager() const {
    return m_ScriptContextManager;
}

#ifdef ENABLE_REPL
LuaREPLServer *TASEngine::GetREPLServer() const {
    return m_REPLServer;
}
#endif

RecordPlayer *TASEngine::GetRecordPlayer() const {
    return m_RecordPlayer;
}

Recorder *TASEngine::GetRecorder() const {
    return m_Recorder;
}

ScriptGenerator *TASEngine::GetScriptGenerator() const {
    return m_ScriptGenerator;
}

StartupProjectManager *TASEngine::GetStartupProjectManager() const {
    return m_StartupProjectManager;
}

RecordingController *TASEngine::GetRecordingController() const {
    return m_RecordingController;
}

PlaybackController *TASEngine::GetPlaybackController() const {
    return m_PlaybackController;
}

TranslationController *TASEngine::GetTranslationController() const {
    return m_TranslationController;
}

TASStateMachine *TASEngine::GetStateMachine() const {
    return m_StateMachine;
}
