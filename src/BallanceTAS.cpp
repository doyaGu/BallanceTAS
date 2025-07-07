#include "BallanceTAS.h"

#include <BML/Bui.h>

#include "physics_RT.h"
#include "TASHook.h"
#include "TASEngine.h"
#include "InGameOSD.h"
#include "Recorder.h"
#include "GameInterface.h"
#include "UIManager.h"

// Global instance pointer required by BML
BallanceTAS *g_Mod;

// BML entry and exit points
MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    g_Mod = new BallanceTAS(bml);
    return g_Mod;
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete static_cast<BallanceTAS *>(mod);
    g_Mod = nullptr;
}

BallanceTAS::BallanceTAS(IBML *bml) : IMod(bml) {
    m_Logger = IMod::GetLogger();
}

BallanceTAS::~BallanceTAS() {
    // Ensure shutdown is called even if OnUnload isn't (e.g., forceful exit)
    if (m_Initialized) {
        Shutdown();
    }

    m_Logger = nullptr;
}

void BallanceTAS::OnLoad() {
    // --- 1. Initialize Configuration ---
    // This is the primary switch for the TAS framework.
    m_Enabled = GetConfig()->GetProperty("TAS", "EnableFramework");
    m_Enabled->SetComment("Enables TAS features (determinism hooks always active for fair gameplay).");
    m_Enabled->SetDefaultBoolean(true);

    // Developer mode for debugging tools.
    m_EnableDeveloperMode = GetConfig()->GetProperty("TAS", "EnableDeveloperMode");
    m_EnableDeveloperMode->SetComment("Enables developer/debug APIs like tas.tools.teleport. Requires restart.");
    m_EnableDeveloperMode->SetDefaultBoolean(false);

    // UI visibility control.
    m_ShowOSD = GetConfig()->GetProperty("OSD", "ShowInGameOSD");
    m_ShowOSD->SetComment("Controls the visibility of the in-game On-Screen Display.");
    m_ShowOSD->SetDefaultBoolean(true);

    // Hotkey configuration
    m_StopKey = GetConfig()->GetProperty("Hotkeys", "StopKey");
    m_StopKey->SetComment("Key for stopping TAS playback or recording");
    m_StopKey->SetDefaultKey(CKKEY_F3);

    m_RecordingKey = GetConfig()->GetProperty("Hotkeys", "RecordingKey");
    m_RecordingKey->SetComment("Key to start/stop recording TAS input");
    m_RecordingKey->SetDefaultKey(CKKEY_F3);

    // --- Recording Configuration ---
    m_DefaultAuthor = GetConfig()->GetProperty("Recording", "DefaultAuthor");
    m_DefaultAuthor->SetComment("Default author name for recorded TAS projects");
    m_DefaultAuthor->SetDefaultString("Player");

    m_RecordingMaxFrames = GetConfig()->GetProperty("Recording", "MaxFrames");
    m_RecordingMaxFrames->SetComment("Maximum frames to record (prevents memory issues)");
    m_RecordingMaxFrames->SetDefaultInteger(100000);

    // --- OSD Panel Configuration ---
    m_ShowOSDStatus = GetConfig()->GetProperty("OSD", "ShowStatusPanel");
    m_ShowOSDStatus->SetComment("Show the status panel (TAS mode, frame count, ground contact)");
    m_ShowOSDStatus->SetDefaultBoolean(true);

    m_ShowOSDVelocity = GetConfig()->GetProperty("OSD", "ShowVelocityPanel");
    m_ShowOSDVelocity->SetComment("Show the velocity panel (speed, velocity components, graphs)");
    m_ShowOSDVelocity->SetDefaultBoolean(true);

    m_ShowOSDPosition = GetConfig()->GetProperty("OSD", "ShowPositionPanel");
    m_ShowOSDPosition->SetComment("Show the position panel (coordinates, trajectory graph)");
    m_ShowOSDPosition->SetDefaultBoolean(true);

    m_ShowOSDPhysics = GetConfig()->GetProperty("OSD", "ShowPhysicsPanel");
    m_ShowOSDPhysics->SetComment("Show the physics panel (angular velocity, mass, physics state)");
    m_ShowOSDPhysics->SetDefaultBoolean(false);

    m_ShowOSDKeys = GetConfig()->GetProperty("OSD", "ShowKeysPanel");
    m_ShowOSDKeys->SetComment("Show the input keys panel (real-time key state display)");
    m_ShowOSDKeys->SetDefaultBoolean(true);

    m_OSDPositionX = GetConfig()->GetProperty("OSD", "PositionX");
    m_OSDPositionX->SetComment("OSD horizontal position (0.0 = left, 1.0 = right)");
    m_OSDPositionX->SetDefaultFloat(0.02f);

    m_OSDPositionY = GetConfig()->GetProperty("OSD", "PositionY");
    m_OSDPositionY->SetComment("OSD vertical position (0.0 = top, 1.0 = bottom)");
    m_OSDPositionY->SetDefaultFloat(0.02f);

    m_OSDOpacity = GetConfig()->GetProperty("OSD", "Opacity");
    m_OSDOpacity->SetComment("OSD transparency (0.0 = transparent, 1.0 = opaque)");
    m_OSDOpacity->SetDefaultFloat(0.9f);

    m_OSDScale = GetConfig()->GetProperty("OSD", "Scale");
    m_OSDScale->SetComment("OSD scale factor (1.0 = normal size)");
    m_OSDScale->SetDefaultFloat(1.0f);

    VxMakeDirectory((CKSTRING) BML_TAS_PATH);

    m_InputManager = m_BML->GetInputManager();

    InitPhysicsMethodPointers();

    // --- 2. Always initialize determinism hooks early ---
    // These must be enabled regardless of user settings for fair gameplay
    if (!InitializeDeterminismHooks()) {
        GetLogger()->Error("Failed to initialize determinism hooks.");
        // Continue anyway - some features may still work
    }

    // --- 3. Initialize TAS Framework if enabled ---
    if (m_Enabled->GetBoolean()) {
        if (!Initialize()) {
            GetLogger()->Error("Failed to initialize BallanceTAS framework.");
            // Framework is disabled due to initialization failure
        }
    }
}

void BallanceTAS::OnUnload() {
    // Shutdown framework
    Shutdown();

    // Clean up determinism hooks too
    try {
        UnhookPhysicsRT();
        UnhookRandom();
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        GetLogger()->Info("Determinism hooks cleaned up.");
    } catch (const std::exception &e) {
        GetLogger()->Error("Exception cleaning up determinism hooks: %s", e.what());
    }
}

void BallanceTAS::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
    // Dynamically enable/disable the framework at runtime based on config changes.
    if (prop == m_Enabled) {
        if (m_Enabled->GetBoolean() && !m_Initialized) {
            GetLogger()->Info("BallanceTAS framework enabled.");
            if (!Initialize()) {
                GetLogger()->Error("Failed to enable BallanceTAS framework.");
            }
        } else if (!m_Enabled->GetBoolean() && m_Initialized) {
            GetLogger()->Info("BallanceTAS framework disabled.");
            Shutdown();
        }
    } else if (prop == m_ShowOSDStatus || prop == m_ShowOSDVelocity ||
    prop == m_ShowOSDPosition || prop == m_ShowOSDPhysics ||
    prop == m_ShowOSDKeys ||
    prop == m_OSDPositionX || prop == m_OSDPositionY ||
    prop == m_OSDOpacity || prop == m_OSDScale) {
        UpdateOSDPanelConfig();
    } else if (prop == m_RecordingKey && m_Initialized) {
        // Update recording hotkey
        if (m_UIManager) {
            m_UIManager->SetRecordingHotkey(m_RecordingKey->GetKey());
        }
    } else if (prop == m_RecordingMaxFrames && m_Initialized) {
        if (m_Engine && m_Engine->GetRecorder()) {
            m_Engine->GetRecorder()->SetMaxFrames(m_RecordingMaxFrames->GetInteger());
        }
    } else if (prop == m_DefaultAuthor && m_Initialized) {
        if (m_Engine && m_Engine->GetRecorder()) {
            m_Engine->GetRecorder()->SetDefaultAuthor(m_DefaultAuthor->GetString());
        }
    }

    // Forward relevant config changes to the engine and UI if they're running.
    if (m_Initialized) {
        if (prop == m_EnableDeveloperMode && m_Engine) {
            m_Engine->SetDeveloperMode(m_EnableDeveloperMode->GetBoolean());
        }
        if (prop == m_ShowOSD) {
            SetOSDVisible(m_ShowOSD->GetBoolean());
        }
    }
}

void BallanceTAS::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                              CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                              XObjectArray *objArray, CKObject *masterObj) {
    if (m_Initialized && m_Engine) {
        if (!strcmp(filename, "3D Entities\\Gameplay.nmo")) {
            m_Engine->GetGameInterface()->AcquireGameplayInfo();
        }

        if (isMap) {
            std::string mapName = filename;
            mapName = mapName.substr(mapName.find_last_of('\\') + 1);
            mapName = mapName.substr(0, mapName.find_last_of('.'));
            m_Engine->GetGameInterface()->SetMapName(mapName);
        }
    }

    if (!strcmp(filename, "3D Entities\\Menu.nmo")) {
        m_Level01 = m_BML->Get2dEntityByName("M_Start_But_01");
        CKBehavior *menuStart = m_BML->GetScriptByName("Menu_Start");
        m_ExitStart = ScriptHelper::FindFirstBB(menuStart, "Exit");
        CKBehavior *menuMain = m_BML->GetScriptByName("Menu_Main");
        m_ExitMain = ScriptHelper::FindFirstBB(menuMain, "Exit", false, 1, 0);
    }
}

void BallanceTAS::OnLoadScript(const char *filename, CKBehavior *script) {
    if (m_Initialized && m_Engine) {
        if (!strcmp(script->GetName(), "Gameplay_Ingame")) {
            for (int i = 0; i < script->GetLocalParameterCount(); ++i) {
                CKParameter *param = script->GetLocalParameter(i);
                if (!strcmp(param->GetName(), "ActiveBall")) {
                    m_Engine->GetGameInterface()->SetActiveBall(param);
                    break;
                }
            }
        }
    }
}

bool BallanceTAS::InitializeDeterminismHooks() {
    GetLogger()->Info("Initializing determinism hooks...");

    // Initialize MinHook if not already done
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        GetLogger()->Error("MinHook failed to initialize: %d", status);
        return false;
    }

    // Always hook physics for determinism
    if (!HookPhysicsRT()) {
        GetLogger()->Error("Failed to hook physics engine for determinism!");
        return false;
    }

    // Always hook random for determinism
    if (!HookRandom()) {
        GetLogger()->Error("Failed to hook random generator for determinism!");
        return false;
    }

    GetLogger()->Info("Determinism hooks initialized successfully.");
    return true;
}

bool BallanceTAS::InitializeGameHooks() {
    if (m_GameHooksEnabled) {
        GetLogger()->Warn("Game hooks already enabled.");
        return true;
    }

    GetLogger()->Info("Enabling game hooks...");

    CKContext* context = GetBML()->GetCKContext();
    if (!context) {
        GetLogger()->Error("Could not get CKContext to find managers.");
        return false;
    }

    bool success = true;

    try {
        auto *timeManager = (CKTimeManager *) context->GetManagerByGuid(TIME_MANAGER_GUID);
        if (!timeManager || !CKTimeManagerHook::Enable(timeManager)) {
            GetLogger()->Error("Failed to enable TimeManager hook.");
            success = false;
        } else {
            GetLogger()->Info("TimeManager hook enabled.");
        }

        auto *inputManager = (CKInputManager *) context->GetManagerByGuid(INPUT_MANAGER_GUID);
        if (!inputManager || !CKInputManagerHook::Enable(inputManager)) {
            GetLogger()->Error("Failed to enable InputManager hook.");
            success = false;
        } else {
            GetLogger()->Info("InputManager hook enabled.");
        }
    } catch (const std::exception &e) {
        GetLogger()->Error("Exception enabling hooks: %s", e.what());
        success = false;
    }

    if (success) {
        m_GameHooksEnabled = true;
        GetLogger()->Info("Game hooks enabled successfully.");
    } else {
        // Cleanup partial success
        DisableGameHooks();
    }

    return success;
}

void BallanceTAS::DisableGameHooks() {
    if (!m_GameHooksEnabled) return;

    GetLogger()->Info("Disabling game hooks...");

    try {
        CKTimeManagerHook::Disable();
        CKInputManagerHook::Disable();
        GetLogger()->Info("Game hooks disabled.");
    } catch (const std::exception &e) {
        GetLogger()->Error("Exception disabling hooks: %s", e.what());
    }

    m_GameHooksEnabled = false;
    // NOTE: We don't call MH_DisableHook(MH_ALL_HOOKS) here because
    // physics hooks should remain active for determinism
}

bool BallanceTAS::Initialize() {
    if (m_Initialized) {
        GetLogger()->Warn("BallanceTAS framework already initialized.");
        return true;
    }

    GetLogger()->Info("Initializing BallanceTAS framework...");

    try {
        // Initialize game hooks first
        if (!InitializeGameHooks()) {
            throw std::runtime_error("Failed to initialize game hooks.");
        }

        // Initialize TAS Engine
        m_Engine = std::make_unique<TASEngine>(this);
        if (!m_Engine->Initialize()) {
            throw std::runtime_error("Engine failed to initialize.");
        }

        // Initialize UI Manager
        m_UIManager = std::make_unique<UIManager>(m_Engine.get());
        if (!m_UIManager->Initialize()) {
            throw std::runtime_error("UIManager failed to initialize.");
        }

        m_Initialized = true;
        GetLogger()->Info("BallanceTAS framework initialized successfully.");

        // Sync initial config states
        m_Engine->SetDeveloperMode(m_EnableDeveloperMode->GetBoolean());
        SetOSDVisible(m_ShowOSD->GetBoolean());
        UpdateOSDPanelConfig();

        // Configure recording settings
        if (auto *recorder = m_Engine->GetRecorder()) {
            recorder->SetDefaultAuthor(m_DefaultAuthor->GetString());
            recorder->SetMaxFrames(m_RecordingMaxFrames->GetInteger());
            recorder->SetAutoGenerate(true); // Always auto-generate
        }

        // Configure recording hotkey
        if (m_UIManager) {
            m_UIManager->SetRecordingHotkey(m_RecordingKey->GetKey());
        }

        return true;
    } catch (const std::exception &e) {
        GetLogger()->Error("Exception during initialization: %s", e.what());

        // Clean up partial initialization
        if (m_UIManager) {
            m_UIManager->Shutdown();
            m_UIManager.reset();
        }
        if (m_Engine) {
            m_Engine->Shutdown();
            m_Engine.reset();
        }
        DisableGameHooks();
        return false;
    }
}

void BallanceTAS::Shutdown() {
    if (!m_Initialized) return;

    GetLogger()->Info("Shutting down BallanceTAS framework...");

    try {
        // Shutdown UI first
        if (m_UIManager) {
            m_UIManager->Shutdown();
            m_UIManager.reset();
        }

        // Then shutdown engine
        if (m_Engine) {
            m_Engine->Shutdown();
            m_Engine.reset();
        }

        // Disable game hooks
        DisableGameHooks();

        GetLogger()->Info("BallanceTAS framework shutdown complete.");
    } catch (const std::exception &e) {
        GetLogger()->Error("Exception during shutdown: %s", e.what());
    }

    m_Initialized = false;
    // NOTE: Determinism hooks remain active for fair gameplay
}

void BallanceTAS::OnMenuStart() {
    if (m_Level01 && m_Level01->IsVisible()) {
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
        ImGui::SetNextWindowPos(ImVec2(vpSize.x * 0.61f, vpSize.y * 0.88f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        constexpr ImGuiWindowFlags ButtonFlags = ImGuiWindowFlags_NoDecoration |
                                                 ImGuiWindowFlags_NoBackground |
                                                 ImGuiWindowFlags_NoMove |
                                                 ImGuiWindowFlags_NoNav |
                                                 ImGuiWindowFlags_AlwaysAutoResize |
                                                 ImGuiWindowFlags_NoBringToFrontOnFocus |
                                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                                 ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("Button_TAS", nullptr, ButtonFlags)) {
            if (Bui::SmallButton("TAS")) {
                m_ExitStart->ActivateInput(0);
                m_ExitStart->Activate();
                if (m_UIManager) {
                    m_UIManager->ToggleTASMenu();
                }
            }
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
    }
}

void BallanceTAS::OnProcess() {
    if (m_Initialized && m_Engine && m_UIManager) {
        OnMenuStart();

        // Process and render UI
        m_UIManager->Process();
        m_UIManager->Render();

        // Handle stop key for both playback and recording
        if (m_Engine->IsPlaying() || m_Engine->IsRecording()) {
            if (m_InputManager && m_InputManager->oIsKeyPressed(m_StopKey->GetKey())) {
                m_Engine->Stop();
                GetLogger()->Info("TAS stopped via stop hotkey.");
            }
        }
    }
}

void BallanceTAS::SetUIMode(int mode) {
    if (m_Initialized && m_UIManager) {
        UIMode uiMode;
        switch (mode) {
        case 0: uiMode = UIMode::Idle; break;
        case 1: uiMode = UIMode::Playing; break;
        case 2: uiMode = UIMode::Recording; break;
        case 3: uiMode = UIMode::Paused; break;
        default: uiMode = UIMode::Idle; break;
        }
        m_UIManager->SetMode(uiMode);
    }
}

void BallanceTAS::SetOSDVisible(bool visible) {
    if (m_Initialized && m_UIManager) {
        m_UIManager->SetOSDVisible(visible);
    }
}

void BallanceTAS::UpdateOSDPanelConfig() {
    if (!m_Initialized || !m_UIManager) return;

    auto *osd = m_UIManager->GetOSD();
    if (!osd) return;

    // Update panel visibility
    osd->SetPanelVisible(OSDPanel::Status, m_ShowOSDStatus->GetBoolean());
    osd->SetPanelVisible(OSDPanel::Velocity, m_ShowOSDVelocity->GetBoolean());
    osd->SetPanelVisible(OSDPanel::Position, m_ShowOSDPosition->GetBoolean());
    osd->SetPanelVisible(OSDPanel::Physics, m_ShowOSDPhysics->GetBoolean());
    osd->SetPanelVisible(OSDPanel::Keys, m_ShowOSDKeys->GetBoolean());

    // Update position
    osd->SetPosition(m_OSDPositionX->GetFloat(), m_OSDPositionY->GetFloat());

    // Update appearance
    osd->SetOpacity(m_OSDOpacity->GetFloat());
    osd->SetScale(m_OSDScale->GetFloat());

    GetLogger()->Info("OSD panel configuration updated.");
}

// --- Event Forwarding Implementations ---

void BallanceTAS::OnPreStartMenu() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("pre_start_menu");
    }
}

void BallanceTAS::OnPostStartMenu() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("post_start_menu");
    }
}

void BallanceTAS::OnExitGame() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("exit_game");
        m_Engine->Stop();
    }

    m_Level01 = nullptr;
}

void BallanceTAS::OnPreLoadLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->Start();
        m_Engine->OnGameEvent("pre_load_level");
    }
}

void BallanceTAS::OnPostLoadLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("post_load_level");
    }
}

void BallanceTAS::OnStartLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("start_level");
    }
}

void BallanceTAS::OnPreResetLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("pre_reset_level");
        m_Engine->Stop();
    }
}

void BallanceTAS::OnPostResetLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("post_reset_level");
    }
}

void BallanceTAS::OnPauseLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("pause_level");
    }
}

void BallanceTAS::OnUnpauseLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("unpause_level");
    }
}

void BallanceTAS::OnPreExitLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("pre_exit_level");
        m_Engine->Stop();
    }
}

void BallanceTAS::OnPostExitLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("post_exit_level");
    }
}

void BallanceTAS::OnPreNextLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("pre_next_level");
    }
}

void BallanceTAS::OnPostNextLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("post_next_level");
    }
}

void BallanceTAS::OnDead() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("ball_off");
    }
}

void BallanceTAS::OnPreEndLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("pre_level_end");
    }
}

void BallanceTAS::OnPostEndLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("post_level_end");
    }
}

void BallanceTAS::OnCounterActive() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("counter_active");
    }
}

void BallanceTAS::OnCounterInactive() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("counter_inactive");
    }
}

void BallanceTAS::OnBallNavActive() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("ball_nav_active");
    }
}

void BallanceTAS::OnBallNavInactive() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("ball_nav_inactive");
    }
}

void BallanceTAS::OnCamNavActive() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("cam_nav_active");
    }
}

void BallanceTAS::OnCamNavInactive() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("cam_nav_inactive");
    }
}

void BallanceTAS::OnBallOff() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("ball_off");
    }
}

void BallanceTAS::OnPreCheckpointReached() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        int sector = -1;
        if (m_Engine->GetGameInterface()) {
            sector = m_Engine->GetGameInterface()->GetCurrentSector();
        }
        m_Engine->OnGameEvent("pre_checkpoint_reached", sector);
    }
}

void BallanceTAS::OnPostCheckpointReached() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        int sector = -1;
        if (m_Engine->GetGameInterface()) {
            sector = m_Engine->GetGameInterface()->GetCurrentSector();
        }
        m_Engine->OnGameEvent("post_checkpoint_reached", sector);
    }
}

void BallanceTAS::OnLevelFinish() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("level_finish");
        m_Engine->Stop();
    }
}

void BallanceTAS::OnGameOver() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("game_over");
    }
}

void BallanceTAS::OnExtraPoint() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("extra_point");
    }
}

void BallanceTAS::OnPreSubLife() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("pre_sub_life");
    }
}

void BallanceTAS::OnPostSubLife() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("post_sub_life");
    }
}

void BallanceTAS::OnPreLifeUp() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("pre_life_up");
    }
}

void BallanceTAS::OnPostLifeUp() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->OnGameEvent("post_life_up");
    }
}