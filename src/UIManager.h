#pragma once

#include <BML/Bui.h>

#include <memory>
#include <string>

// Forward declarations
class BallanceTAS;
class IBML;
class TASEngine;
class TASMenu;
class InGameOSD;
enum class OSDPanel;

/**
 * @enum UIMode
 * @brief Represents the current high-level state of the UI for display purposes.
 */
enum class UIMode {
    Idle      = 0,
    Playing   = 1,
    Paused    = 2,
    Recording = 3 // New recording mode
};

/**
 * @class UIManager
 * @brief Manages all user interface components for the TAS framework.
 *
 * This class provides a centralized system for managing TAS-related UI components,
 * handling their lifecycle, state synchronization, and rendering coordination.
 * It serves as the main interface between the TAS engine and the visual components.
 * UIManager is owned by BallanceTAS and receives a TASEngine reference for data access.
 */
class UIManager {
public:
    explicit UIManager(TASEngine *engine);
    ~UIManager();

    // UIManager is not copyable or movable
    UIManager(const UIManager &) = delete;
    UIManager &operator=(const UIManager &) = delete;

    /**
     * @brief Initializes all UI components and subsystems.
     * @return True on success, false on failure.
     */
    bool Initialize();

    /**
     * @brief Shuts down and cleans up all UI resources.
     */
    void Shutdown();

    /**
     * @brief Processes UI logic and input handling.
     * Should be called every frame before rendering.
     */
    void Process();

    /**
     * @brief Renders all active UI components.
     * Should be called during the game's render phase.
     */
    void Render();

    // --- TAS Menu Control ---

    /**
     * @brief Opens the main TAS Menu.
     */
    void OpenTASMenu();

    /**
     * @brief Closes the TAS Menu if open.
     */
    void CloseTASMenu();

    /**
     * @brief Toggles the visibility of the main TAS Menu.
     */
    void ToggleTASMenu();

    /**
     * @brief Checks if the TAS menu is currently open.
     * @return True if any TAS menu page is visible.
     */
    bool IsTASMenuOpen() const;

    // --- Recording Control ---

    /**
     * @brief Starts recording a new TAS.
     * @return True if recording started successfully.
     */
    bool StartRecording();

    /**
     * @brief Stops recording and prompts for script generation.
     * @param projectName Optional project name (will prompt if empty).
     * @return True if recording was stopped.
     */
    bool StopRecording(const std::string &projectName = "");

    /**
     * @brief Toggles recording state.
     */
    void ToggleRecording();

    /**
     * @brief Checks if currently recording.
     * @return True if recording is active.
     */
    bool IsRecording() const;

    // --- OSD Control ---

    /**
     * @brief Sets the visibility of the In-Game OSD.
     * @param visible True to show, false to hide.
     */
    void SetOSDVisible(bool visible);

    /**
     * @brief Toggles the In-Game OSD visibility.
     */
    void ToggleOSD();

    /**
     * @brief Checks if the OSD is currently visible.
     * @return True if OSD is visible.
     */
    bool IsOSDVisible() const;

    /**
     * @brief Toggles a specific OSD panel.
     * @param panel The panel to toggle.
     */
    void ToggleOSDPanel(OSDPanel panel);

    /**
     * @brief Cycles the trajectory view plane (XZ -> XY -> YZ).
     */
    void CycleTrajectoryPlane();

    // --- State Management ---

    /**
     * @brief Updates the current UI mode across all components.
     * @param mode The new UIMode (Playing, Recording, etc.).
     */
    void SetMode(UIMode mode);

    /**
     * @brief Gets the current UI mode.
     * @return The current UIMode.
     */
    UIMode GetMode() const { return m_CurrentMode; }

    // --- Component Access ---

    /**
     * @brief Gets the TAS menu instance.
     * @return Pointer to the TAS menu, or nullptr if not initialized.
     */
    TASMenu *GetTASMenu() const { return m_TASMenu.get(); }

    /**
     * @brief Gets the In-Game OSD instance.
     * @return Pointer to the OSD, or nullptr if not initialized.
     */
    InGameOSD *GetOSD() const { return m_InGameOSD.get(); }

    // --- Configuration ---

    /**
     * @brief Sets the hotkey for toggling the OSD.
     * @param key The key code (default: CKKEY_F11).
     */
    void SetOSDHotkey(CKKEYBOARD key) { m_OSDHotkey = key; }

    /**
     * @brief Sets the hotkey for starting/stopping recording.
     * @param key The key code (default: CKKEY_F3).
     */
    void SetRecordingHotkey(CKKEYBOARD key) { m_RecordingHotkey = key; }

    /**
     * @brief Sets the hotkey for toggling the status panel.
     * @param key The key code (default: CKKEY_F5).
     */
    void SetStatusPanelHotkey(CKKEYBOARD key) { m_StatusPanelHotkey = key; }

    /**
     * @brief Sets the hotkey for toggling the velocity panel.
     * @param key The key code (default: CKKEY_F6).
     */
    void SetVelocityPanelHotkey(CKKEYBOARD key) { m_VelocityPanelHotkey = key; }

    /**
     * @brief Sets the hotkey for toggling the position panel.
     * @param key The key code (default: CKKEY_F7).
     */
    void SetPositionPanelHotkey(CKKEYBOARD key) { m_PositionPanelHotkey = key; }

    /**
     * @brief Sets the hotkey for toggling the physics panel.
     * @param key The key code (default: CKKEY_F8).
     */
    void SetPhysicsPanelHotkey(CKKEYBOARD key) { m_PhysicsPanelHotkey = key; }

    /**
     * @brief Sets the hotkey for cycling trajectory plane.
     * @param key The key code (default: CKKEY_F9).
     */
    void SetTrajectoryPlaneHotkey(CKKEYBOARD key) { m_TrajectoryPlaneHotkey = key; }

private:
    // --- Internal Methods ---
    void UpdateHotkeys();
    void HandleRecordingHotkey();

    // --- Core References ---
    TASEngine *m_Engine;
    BallanceTAS *m_Mod;
    IBML *m_BML;

    // --- UI Components ---
    std::unique_ptr<TASMenu> m_TASMenu;
    std::unique_ptr<InGameOSD> m_InGameOSD;

    // --- State ---
    bool m_Initialized = false;
    bool m_OSDVisible = true;
    UIMode m_CurrentMode = UIMode::Idle;

    // --- Configuration ---
    CKKEYBOARD m_OSDHotkey = CKKEY_F11;
    CKKEYBOARD m_RecordingHotkey = CKKEY_F3;
    CKKEYBOARD m_StatusPanelHotkey = CKKEY_F5;
    CKKEYBOARD m_VelocityPanelHotkey = CKKEY_F6;
    CKKEYBOARD m_PositionPanelHotkey = CKKEY_F7;
    CKKEYBOARD m_PhysicsPanelHotkey = CKKEY_F8;
    CKKEYBOARD m_KeysPanelHotkey = CKKEY_F4;
    CKKEYBOARD m_TrajectoryPlaneHotkey = CKKEY_F9;

    // --- Recording UI State ---
    bool m_ShowRecordingPrompt = false;
    char m_RecordingProjectName[256] = "Generated_TAS";
    float m_RecordingPromptTimer = 0.0f;
    static constexpr float RECORDING_PROMPT_TIMEOUT = 10.0f; // 10 seconds to respond
};
