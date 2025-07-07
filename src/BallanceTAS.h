#pragma once

#include <BML/BMLAll.h>
#include <memory>

#define BML_TAS_PATH "..\\ModLoader\\TAS\\"

// Forward-declare the core engine to avoid including its full header here.
// This reduces compile times and keeps dependencies clean.
class TASEngine;
class UIManager;

/**
 * @class BallanceTAS
 * @brief The main BML Mod class for the BallanceTAS framework.
 *
 * This class serves as the entry point for the BML loader. It is responsible for:
 * 1. Initializing and shutting down the core TASEngine and UIManager.
 * 2. Handling BML configuration properties.
 * 3. Receiving game lifecycle events from BML (e.g., OnStartLevel) and
 *    forwarding them to the appropriate systems within the framework.
 * 4. Driving the core engine's Tick() and UI Render() methods every frame.
 * 5. Coordinating between TAS logic and UI systems.
 */
class BallanceTAS : public IMod {
public:
    explicit BallanceTAS(IBML *bml);
    ~BallanceTAS() override;

    //================================================================
    // IMod Interface Implementation
    //================================================================

    const char *GetID() override { return "BallanceTAS"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Ballance TAS"; }
    const char *GetAuthor() override { return "Ballance TAS Community"; }

    const char *GetDescription() override {
        return "A tool-assisted speedrun framework for Ballance with recording capabilities.";
    }

    DECLARE_BML_VERSION;

    // Core lifecycle methods
    void OnLoad() override;
    void OnUnload() override;

    // Per-frame processing
    void OnProcess() override;

    // Configuration handling
    void OnModifyConfig(const char *category, const char *key, IProperty *prop) override;

    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                  CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                  XObjectArray *objArray, CKObject *masterObj) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;

    //================================================================
    // Game Event Forwarding
    //================================================================

    void OnPreStartMenu() override;
    void OnPostStartMenu() override;

    void OnExitGame() override;

    void OnPreLoadLevel() override;
    void OnPostLoadLevel() override;

    void OnStartLevel() override;

    void OnPreResetLevel() override;
    void OnPostResetLevel() override;

    void OnPauseLevel() override;
    void OnUnpauseLevel() override;

    void OnPreExitLevel() override;
    void OnPostExitLevel() override;

    void OnPreNextLevel() override;
    void OnPostNextLevel() override;

    void OnDead() override;

    void OnPreEndLevel() override;
    void OnPostEndLevel() override;

    void OnCounterActive() override;
    void OnCounterInactive() override;

    void OnBallNavActive() override;
    void OnBallNavInactive() override;

    void OnCamNavActive() override;
    void OnCamNavInactive() override;

    void OnBallOff() override;

    void OnPreCheckpointReached() override;
    void OnPostCheckpointReached() override;

    void OnLevelFinish() override;

    void OnGameOver() override;

    void OnExtraPoint() override;

    void OnPreSubLife() override;
    void OnPostSubLife() override;

    void OnPreLifeUp() override;
    void OnPostLifeUp() override;

    //================================================================
    // Public Accessors
    //================================================================

    IBML *GetBML() const { return m_BML; }
    ILogger *GetLogger() const { return m_Logger; }
    InputHook *GetInputManager() const { return m_InputManager; }
    UIManager *GetUIManager() const { return m_UIManager.get(); }
    TASEngine *GetEngine() const { return m_Engine.get(); }

    //================================================================
    // UI Coordination Methods
    //================================================================

    /**
     * @brief Sets the UI mode (called by TASEngine to update UI state).
     * @param mode The new UI mode.
     */
    void SetUIMode(int mode);

    /**
     * @brief Sets OSD visibility (called via config changes).
     * @param visible Whether the OSD should be visible.
     */
    void SetOSDVisible(bool visible);

    /**
     * @brief Updates OSD panel configuration based on config changes.
     */
    void UpdateOSDPanelConfig();

private:
    /**
     * @brief Initializes determinism hooks that are always active for fair gameplay.
     * Called during OnLoad() regardless of user settings.
     * @return True if hooks were initialized successfully.
     */
    bool InitializeDeterminismHooks();

    /**
     * @brief Initializes the game hooks for TAS functionality.
     * Called during framework initialization.
     * @return True if hooks were initialized successfully.
     */
    bool InitializeGameHooks();

    /**
     * @brief Disables the game hooks for TAS functionality.
     * Called during framework shutdown.
     */
    void DisableGameHooks();

    /**
     * @brief Initializes the TASEngine and UIManager and all their subsystems.
     * This is called from OnLoad or when the mod is enabled via config.
     */
    bool Initialize();

    /**
     * @brief Shuts down and cleans up the TASEngine and UIManager.
     * This is called from OnUnload or when the mod is disabled.
     */
    void Shutdown();

    void OnMenuStart();

    // The single, top-level instance of the TAS framework's core engine.
    std::unique_ptr<TASEngine> m_Engine;

    // UI Manager for all TAS-related user interface components.
    std::unique_ptr<UIManager> m_UIManager;

    bool m_Initialized = false;
    bool m_GameHooksEnabled = false;

    ILogger *m_Logger = nullptr;
    InputHook *m_InputManager = nullptr;

    CK2dEntity *m_Level01 = nullptr;
    CKBehavior *m_ExitStart = nullptr;
    CKBehavior *m_ExitMain = nullptr;

    // --- Configuration Properties ---
    // These pointers are owned by BML's config manager.
    IProperty *m_Enabled = nullptr;
    IProperty *m_EnableDeveloperMode = nullptr;
    IProperty *m_ShowOSD = nullptr;
    IProperty *m_StopKey = nullptr;

    IProperty *m_ShowOSDStatus = nullptr;
    IProperty *m_ShowOSDVelocity = nullptr;
    IProperty *m_ShowOSDPosition = nullptr;
    IProperty *m_ShowOSDPhysics = nullptr;
    IProperty *m_ShowOSDKeys = nullptr;
    IProperty *m_OSDPositionX = nullptr;
    IProperty *m_OSDPositionY = nullptr;
    IProperty *m_OSDOpacity = nullptr;
    IProperty *m_OSDScale = nullptr;

    // --- Recording Configuration ---
    IProperty *m_RecordingMaxFrames = nullptr;
};