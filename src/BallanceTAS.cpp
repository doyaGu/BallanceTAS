#include "BallanceTAS.h"

#include <BML/Bui.h>

#include "physics_RT.h"
#include "TASHook.h"
#include "TASEngine.h"
#include "InGameOSD.h"
#include "Recorder.h"
#include "GameInterface.h"
#include "ServiceContainer.h"
#include "ScriptContextManager.h"
#include "StartupProjectManager.h"
#include "UIManager.h"
#include "Logger.h"
#include "BMLLogSink.h"
#include "EventBus.h"
#include "GameEvents.h"
#include "HookManager.h"

template <typename Event>
static void PublishEngineEvent(BallanceTAS *mod, const Event &event) {
    if (!mod) {
        return;
    }

    auto *engine = mod->GetEngine();
    auto *eventBus = mod->GetEventBus();
    if (engine && eventBus && !engine->IsShuttingDown()) {
        eventBus->Publish(event);
    }
}

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

    // Shutdown global logger system (safe to call multiple times)
    Log::Shutdown();

    m_Logger = nullptr;
}

void BallanceTAS::OnLoad() {
    // Initialize global logger system via the BML adapter
    m_LogSink = std::make_unique<BMLLogSink>(GetLogger());
    Log::Initialize(m_LogSink.get());

    // --- 1. Initialize Configuration ---
    m_ConfigService.RegisterProperties(GetConfig());

    m_InputManager = m_BML->GetInputManager();

    InitPhysicsAddresses();

    // Initialize MinHook
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        Log::Error("MinHook failed to initialize: %s", MH_StatusToString(status));
    }

    m_EventBus = std::make_unique<EventBus>();
    m_HookManager = std::make_unique<HookManager>();

    if (!InitializeGameHooks()) {
        Log::Error("Failed to initialize shared game hooks.");
    }

    // --- 2. Initialize TAS Framework if enabled ---
    if (m_ConfigService.IsEnabled()) {
        if (!Initialize()) {
            Log::Error("Failed to initialize BallanceTAS framework.");
            // Framework is disabled due to initialization failure
        }
    }
}

void BallanceTAS::OnUnload() {
    // Shutdown framework
    Shutdown();

    DisableGameHooks();
    m_HookManager.reset();
    m_EventBus.reset();

    MH_Uninitialize();

    // Shutdown global logger system
    Log::Shutdown();
}

void BallanceTAS::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
    // Notify ConfigService (publishes ConfigChangedEvent on the EventBus).
    m_ConfigService.OnPropertyChanged(category, key, prop);

    // Handle enable/disable toggle directly since it controls initialization lifecycle.
    if (std::string_view(category) == "TAS" && std::string_view(key) == "Enable") {
        if (m_ConfigService.IsEnabled() && !m_Initialized) {
            Log::Info("BallanceTAS framework enabled.");
            if (!Initialize()) {
                Log::Error("Failed to enable BallanceTAS framework.");
            }
        } else if (!m_ConfigService.IsEnabled() && m_Initialized) {
            Log::Info("BallanceTAS framework disabled.");
            Shutdown();
        }
        return;
    }

    if (!m_Initialized) return;

    // Sync settings that need direct forwarding to subsystems.
    if (std::string_view(category) == "TAS") {
        if (m_Engine) {
            m_Engine->SetValidationEnabled(m_ConfigService.IsValidation());
            m_Engine->SetAutoRestartEnabled(m_ConfigService.IsAutoRestart());
        }
    } else if (std::string_view(category) == "Recording") {
        if (auto *recorder = m_Engine ? m_Engine->GetServiceProvider().Resolve<Recorder>() : nullptr) {
            recorder->SetMaxFrames(m_ConfigService.GetMaxFrames());
        }
    } else if (std::string_view(category) == "Hotkeys") {
        if (m_UIManager) {
            m_UIManager->SetStopHotkey(m_ConfigService.GetStopKey());
        }
    } else if (std::string_view(category) == "OSD") {
        SetOSDVisible(m_ConfigService.IsOSDVisible());
        UpdateOSDPanelConfig();
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
        if (!strcmp(script->GetName(), "Ball_Explosion_Wood")
            || !strcmp(script->GetName(), "Ball_Explosion_Paper")
            || !strcmp(script->GetName(), "Ball_Explosion_Stone")) {
            CKBehavior *beh = ScriptHelper::FindFirstBB(script, "Set Position");
            ScriptHelper::DeleteBB(script, beh);
        }

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

bool BallanceTAS::InitializeGameHooks() {
    if (m_GameHooksEnabled) {
        Log::Warn("Game hooks already enabled.");
        return true;
    }

    Log::Info("Enabling game hooks...");

    CKContext *context = GetBML()->GetCKContext();
    if (!context) {
        Log::Error("Could not get CKContext to find managers.");
        return false;
    }

    if (!m_HookManager) {
        Log::Error("HookManager is not available.");
        return false;
    }

    bool success = true;

    try {
        auto *timeManager = (CKTimeManager *) context->GetManagerByGuid(TIME_MANAGER_GUID);
        if (!timeManager || !m_HookManager->EnableTimeManagerHook(timeManager)) {
            Log::Error("Failed to enable TimeManager hook.");
            success = false;
        } else {
            Log::Info("TimeManager hook enabled.");
        }

        auto *inputManager = (CKInputManager *) context->GetManagerByGuid(INPUT_MANAGER_GUID);
        if (!inputManager || !m_HookManager->EnableInputManagerHook(inputManager)) {
            Log::Error("Failed to enable InputManager hook.");
            success = false;
        } else {
            Log::Info("InputManager hook enabled.");
        }
    } catch (const std::exception &e) {
        Log::Error("Exception enabling hooks: %s", e.what());
        success = false;
    }

    if (success) {
        m_GameHooksEnabled = true;
        Log::Info("Game hooks enabled successfully.");
    } else {
        // Cleanup partial success
        DisableGameHooks();
    }

    return success;
}

void BallanceTAS::DisableGameHooks() {
    if (!m_GameHooksEnabled) return;

    Log::Info("Disabling game hooks...");

    try {
        if (m_HookManager) {
            m_HookManager->DisableAll();
        }

        Log::Info("Game hooks disabled.");
    } catch (const std::exception &e) {
        Log::Error("Exception disabling hooks: %s", e.what());
    }

    m_GameHooksEnabled = false;
}

bool BallanceTAS::Initialize() {
    if (m_Initialized) {
        Log::Warn("BallanceTAS framework already initialized.");
        return true;
    }

    Log::Info("Initializing BallanceTAS framework...");

    try {
        if (!m_GameHooksEnabled || !m_EventBus || !m_HookManager) {
            throw std::runtime_error("Shared runtime infrastructure is not available.");
        }

        m_GameInterface = std::make_unique<GameInterface>(this);

        // Initialize TAS Engine
        m_Engine = std::make_unique<TASEngine>(m_GameInterface.get(), m_EventBus.get(), m_HookManager.get());
        if (!m_Engine->Initialize()) {
            throw std::runtime_error("Engine failed to initialize.");
        }

        m_ConfigService.SetEventBus(m_Engine->GetEventBus());
        m_Engine->SetValidationEnabled(m_ConfigService.IsValidation());
        m_Engine->SetAutoRestartEnabled(m_ConfigService.IsAutoRestart());
        if (auto *startup = m_Engine->GetServiceProvider().Resolve<StartupProjectManager>()) {
            StartupConfig startupConfig = m_ConfigService.GetStartupConfig();
            startup->SetStartupEnabled(startupConfig.enabled);
            startup->SetAutoLoadEnabled(startupConfig.autoLoad);
            if (!startupConfig.project.empty()) {
                startup->SetStartupProject(startupConfig.project);
            }
            if (startupConfig.enabled && startupConfig.autoLoad) {
                startup->LoadAndExecuteStartupScript();
            }
        }

        // Initialize UI Manager
        m_UIManager = std::make_unique<UIManager>(m_Engine.get());
        if (!m_UIManager->Initialize()) {
            throw std::runtime_error("UIManager failed to initialize.");
        }
        m_UIManager->SetStopHotkey(m_ConfigService.GetStopKey());

        m_Initialized = true;
        Log::Info("BallanceTAS framework initialized successfully.");

        // Sync initial config states
        SetOSDVisible(m_ConfigService.IsOSDVisible());
        UpdateOSDPanelConfig();

        // Configure recording settings
        if (auto *recorder = m_Engine->GetServiceProvider().Resolve<Recorder>()) {
            recorder->SetMaxFrames(m_ConfigService.GetMaxFrames());
            recorder->SetAutoGenerate(true); // Always auto-generate
        }

        return true;
    } catch (const std::exception &e) {
        Log::Error("Exception during initialization: %s", e.what());

        // Clean up partial initialization
        if (m_UIManager) {
            m_UIManager->Shutdown();
            m_UIManager.reset();
        }
        if (m_Engine) {
            m_Engine->Shutdown();
            m_Engine.reset();
        }
        if (m_GameInterface) {
            m_GameInterface.reset();
        }
        m_ConfigService.SetEventBus(nullptr);
        return false;
    }
}

void BallanceTAS::Shutdown() {
    if (!m_Initialized) return;

    Log::Info("Shutting down BallanceTAS framework...");

    try {
        m_ConfigService.SetEventBus(nullptr);

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

        Log::Info("BallanceTAS framework shutdown complete.");
    } catch (const std::exception &e) {
        Log::Error("Exception during shutdown: %s", e.what());
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

        m_Engine->IncrementCurrentTick();
        if (!m_Engine->IsPlayingScript()) {
            if (auto *contexts = m_Engine->GetServiceProvider().Resolve<ScriptContextManager>()) {
                contexts->TickAll();
            }
        }

        // Process and render UI
        m_UIManager->Process();
        m_UIManager->Render();
    }

    if (m_SkipRenderingCount != 0) {
        m_BML->SkipRenderForNextTick();
        --m_SkipRenderingCount;
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

    OSDConfig cfg = m_ConfigService.GetOSDConfig();

    osd->SetPanelVisible(OSDPanel::Status, cfg.showStatus);
    osd->SetPanelVisible(OSDPanel::Velocity, cfg.showVelocity);
    osd->SetPanelVisible(OSDPanel::Position, cfg.showPosition);
    osd->SetPanelVisible(OSDPanel::Physics, cfg.showPhysics);
    osd->SetPanelVisible(OSDPanel::Keys, cfg.showKeys);

    osd->SetPosition(cfg.positionX, cfg.positionY);

    osd->SetOpacity(cfg.opacity);
    osd->SetScale(cfg.scale);

    Log::Info("OSD panel configuration updated.");
}

void BallanceTAS::SkipRenderingForTicks(size_t ticks) {
    m_SkipRenderingCount = ticks;
}

// --- Event Forwarding Implementations ---

void BallanceTAS::OnPreStartMenu() {
    PublishEngineEvent(this, PreStartMenuEvent{});
}

void BallanceTAS::OnPostStartMenu() {
    PublishEngineEvent(this, PostStartMenuEvent{});
}

void BallanceTAS::OnExitGame() {
    m_Level01 = nullptr;
    if (m_EventBus) {
        m_EventBus->Publish(ExitGameEvent{});
    }
}

void BallanceTAS::OnPreLoadLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        m_Engine->Start();
    }
    PublishEngineEvent(this, PreLoadLevelEvent{});
}

// Macro for trivial BML callback -> EventBus forwarding
#define FORWARD_SIMPLE_EVENT(BmlCallback, EventType) \
    void BallanceTAS::BmlCallback() { PublishEngineEvent(this, EventType{}); }

FORWARD_SIMPLE_EVENT(OnPostLoadLevel,   PostLoadLevelEvent)
FORWARD_SIMPLE_EVENT(OnStartLevel,      StartLevelEvent)

void BallanceTAS::OnPreResetLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        PublishEngineEvent(this, PreResetLevelEvent{});
        m_Engine->Stop();
    }
}

FORWARD_SIMPLE_EVENT(OnPostResetLevel,  PostResetLevelEvent)
FORWARD_SIMPLE_EVENT(OnPauseLevel,      PauseLevelEvent)
FORWARD_SIMPLE_EVENT(OnUnpauseLevel,    UnpauseLevelEvent)

void BallanceTAS::OnPreExitLevel() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        PublishEngineEvent(this, PreExitLevelEvent{});
        m_Engine->Stop();
    }
}

FORWARD_SIMPLE_EVENT(OnPostExitLevel,   PostExitLevelEvent)
FORWARD_SIMPLE_EVENT(OnPreNextLevel,    PreNextLevelEvent)
FORWARD_SIMPLE_EVENT(OnPostNextLevel,   PostNextLevelEvent)
FORWARD_SIMPLE_EVENT(OnDead,            DeadEvent)
FORWARD_SIMPLE_EVENT(OnPreEndLevel,     PreEndLevelEvent)
FORWARD_SIMPLE_EVENT(OnPostEndLevel,    PostEndLevelEvent)
FORWARD_SIMPLE_EVENT(OnCounterActive,   CounterActiveEvent)
FORWARD_SIMPLE_EVENT(OnCounterInactive, CounterInactiveEvent)
FORWARD_SIMPLE_EVENT(OnBallNavActive,   BallNavActiveEvent)
FORWARD_SIMPLE_EVENT(OnBallNavInactive, BallNavInactiveEvent)
FORWARD_SIMPLE_EVENT(OnCamNavActive,    CamNavActiveEvent)
FORWARD_SIMPLE_EVENT(OnCamNavInactive,  CamNavInactiveEvent)
FORWARD_SIMPLE_EVENT(OnBallOff,         BallOffEvent)
FORWARD_SIMPLE_EVENT(OnGameOver,        GameOverEvent)

#undef FORWARD_SIMPLE_EVENT

void BallanceTAS::OnPreCheckpointReached() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        int sector = -1;
        if (m_Engine->GetGameInterface()) {
            sector = m_Engine->GetGameInterface()->GetCurrentSector();
        }
        PublishEngineEvent(this, PreCheckpointReachedEvent{sector});
    }
}

void BallanceTAS::OnPostCheckpointReached() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        int sector = -1;
        if (m_Engine->GetGameInterface()) {
            sector = m_Engine->GetGameInterface()->GetCurrentSector();
        }
        PublishEngineEvent(this, PostCheckpointReachedEvent{sector});
    }
}

void BallanceTAS::OnLevelFinish() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        PublishEngineEvent(this, LevelFinishEvent{});
        if (m_ConfigService.IsStopOnFinish()) {
            m_Engine->Stop();
        }
    }
}

void BallanceTAS::OnExtraPoint() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        int points = 0;
        if (m_Engine->GetGameInterface()) {
            points = m_Engine->GetGameInterface()->GetPoints();
        }
        PublishEngineEvent(this, ExtraPointEvent{points});
    }
}

void BallanceTAS::OnPreSubLife() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        int lifeCount = 0;
        if (m_Engine->GetGameInterface()) {
            lifeCount = m_Engine->GetGameInterface()->GetLifeCount();
        }
        PublishEngineEvent(this, PreSubLifeEvent{lifeCount});
    }
}

void BallanceTAS::OnPostSubLife() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        int lifeCount = 0;
        if (m_Engine->GetGameInterface()) {
            lifeCount = m_Engine->GetGameInterface()->GetLifeCount();
        }
        PublishEngineEvent(this, PostSubLifeEvent{lifeCount});
    }
}

void BallanceTAS::OnPreLifeUp() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        int lifeCount = 0;
        if (m_Engine->GetGameInterface()) {
            lifeCount = m_Engine->GetGameInterface()->GetLifeCount();
        }
        PublishEngineEvent(this, PreLifeUpEvent{lifeCount});
    }
}

void BallanceTAS::OnPostLifeUp() {
    if (m_Initialized && m_Engine && !m_Engine->IsShuttingDown()) {
        int lifeCount = 0;
        if (m_Engine->GetGameInterface()) {
            lifeCount = m_Engine->GetGameInterface()->GetLifeCount();
        }
        PublishEngineEvent(this, PostLifeUpEvent{lifeCount});
    }
}
