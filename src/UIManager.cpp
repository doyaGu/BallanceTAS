#include "UIManager.h"

#include "BallanceTAS.h"
#include "TASMenu.h"
#include "InGameOSD.h"
#include "TASEngine.h"

UIManager::UIManager(TASEngine *engine)
    : m_Engine(engine), m_Mod(engine ? engine->GetMod() : nullptr), m_BML(m_Mod ? m_Mod->GetBML() : nullptr) {}

UIManager::~UIManager() {
    Shutdown();
}

bool UIManager::Initialize() {
    if (m_Initialized) {
        if (m_Mod && m_Mod->GetLogger()) {
            m_Mod->GetLogger()->Warn("UIManager already initialized.");
        }
        return true;
    }

    if (!m_Engine || !m_Mod || !m_BML) {
        if (m_Mod && m_Mod->GetLogger()) {
            m_Mod->GetLogger()->Error("UIManager missing required dependencies.");
        }
        return false;
    }

    m_Mod->GetLogger()->Info("Initializing UIManager...");

    // Create and initialize TAS menu
    try {
        m_TASMenu = std::make_unique<TASMenu>(m_Engine);
        m_TASMenu->Init();
        m_Mod->GetLogger()->Info("TAS Menu initialized.");
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Failed to initialize TAS Menu: %s", e.what());
        return false;
    }

    // Create and initialize In-Game OSD
    try {
        m_InGameOSD = std::make_unique<InGameOSD>("TAS OSD", m_Engine);
        m_InGameOSD->SetVisibility(false); // Start hidden
        m_Mod->GetLogger()->Info("In-Game OSD initialized.");
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Failed to initialize In-Game OSD: %s", e.what());
        return false;
    }

    m_Initialized = true;
    m_Mod->GetLogger()->Info("UIManager initialized successfully.");
    return true;
}

void UIManager::Shutdown() {
    if (!m_Initialized) return;

    if (m_Mod && m_Mod->GetLogger()) {
        m_Mod->GetLogger()->Info("Shutting down UIManager...");
    }

    // Shutdown components in reverse order
    if (m_InGameOSD) {
        m_InGameOSD->SetVisibility(false);
        m_InGameOSD.reset();
    }

    if (m_TASMenu) {
        m_TASMenu->Shutdown();
        m_TASMenu.reset();
    }

    m_Initialized = false;

    if (m_Mod && m_Mod->GetLogger()) {
        m_Mod->GetLogger()->Info("UIManager shutdown complete.");
    }
}

void UIManager::Process() {
    if (!m_Initialized || !m_Engine || m_Engine->IsShuttingDown()) return;

    UpdateHotkeys();

    // Update OSD visibility based on game state
    if (m_InGameOSD && m_BML) {
        bool shouldShowOSD = m_BML->IsIngame() && m_OSDVisible;
        m_InGameOSD->SetVisibility(shouldShowOSD);

        if (shouldShowOSD) {
            m_InGameOSD->Update(); // Allow OSD to update its data
        }
    }

    // Close TAS menu if we're in game (similar to MapMenu behavior)
    if (m_BML && m_BML->IsIngame() && IsTASMenuOpen()) {
        CloseTASMenu();
    }
}

void UIManager::Render() {
    if (!m_Initialized || !m_Engine || m_Engine->IsShuttingDown()) return;

    // Use Bui's ImGui context scope for proper rendering
    Bui::ImGuiContextScope scope;

    // Render TAS menu
    if (m_TASMenu) {
        m_TASMenu->Render();
    }

    // Render OSD
    if (m_InGameOSD) {
        m_InGameOSD->Render();
    }
}

void UIManager::OpenTASMenu() {
    if (!m_TASMenu || !m_BML || m_BML->IsIngame()) return;

    m_TASMenu->Open("TAS Projects");
    if (m_Mod) {
        m_Mod->GetLogger()->Info("TAS Menu opened.");
    }
}

void UIManager::CloseTASMenu() {
    if (!m_TASMenu) return;

    m_TASMenu->Close();
    if (m_Mod) {
        m_Mod->GetLogger()->Info("TAS Menu closed.");
    }
}

void UIManager::ToggleTASMenu() {
    if (IsTASMenuOpen()) {
        CloseTASMenu();
    } else {
        OpenTASMenu();
    }
}

bool UIManager::IsTASMenuOpen() const {
    if (!m_TASMenu) return false;

    // Check if any TAS menu page is visible
    auto *listPage = m_TASMenu->GetPage("TAS Projects");
    auto *detailsPage = m_TASMenu->GetPage("TAS Details");
    auto *recordPage = m_TASMenu->GetPage("Record New TAS");
    auto *statusPage = m_TASMenu->GetPage("Recording Status");

    return (listPage && listPage->IsVisible()) ||
           (detailsPage && detailsPage->IsVisible()) ||
           (recordPage && recordPage->IsVisible()) ||
           (statusPage && statusPage->IsVisible());
}

// === Recording Control ===

bool UIManager::StartRecording() {
    if (!m_Engine) return false;

    bool success = m_Engine->StartRecording();
    if (success && m_Mod) {
        m_Mod->GetLogger()->Info("Recording started via UI.");
    }
    return success;
}

bool UIManager::StopRecording() {
    if (!m_Engine) return false;

    if (!m_Engine->IsRecording() && !m_Engine->IsPendingRecord()) {
        return false;
    }

    m_Engine->StopRecording();
    if (m_Mod) {
        m_Mod->GetLogger()->Info("Recording stopped via UI.");
    }
    return true;
}

void UIManager::ToggleRecording() {
    if (IsRecording()) {
        StopRecording();
    } else {
        StartRecording();
    }
}

bool UIManager::IsRecording() const {
    return m_Engine && (m_Engine->IsRecording() || m_Engine->IsPendingRecord());
}

// === Replay Control ===

bool UIManager::StartReplay() {
    if (!m_Engine) return false;

    bool success = m_Engine->StartReplay();
    if (success && m_Mod) {
        m_Mod->GetLogger()->Info("Replay started via UI.");
    }
    return success;
}

bool UIManager::StopReplay() {
    if (!m_Engine) return false;

    if (!m_Engine->IsPlaying() && !m_Engine->IsPendingPlay()) {
        return false;
    }

    m_Engine->StopReplay();
    if (m_Mod) {
        m_Mod->GetLogger()->Info("Replay stopped via UI.");
    }
    return true;
}

void UIManager::ToggleReplay() {
    if (IsReplaying()) {
        StopReplay();
    } else {
        StartReplay();
    }
}

bool UIManager::IsReplaying() const {
    return m_Engine && (m_Engine->IsPlaying() || m_Engine->IsPendingPlay());
}

// === OSD Control ===

void UIManager::SetOSDVisible(bool visible) {
    m_OSDVisible = visible;

    if (m_InGameOSD && m_BML) {
        // Only actually show if we're in game
        bool shouldShow = visible && m_BML->IsIngame();
        m_InGameOSD->SetVisibility(shouldShow);
    }

    if (m_Mod) {
        m_Mod->GetLogger()->Info("OSD visibility set to %s", visible ? "visible" : "hidden");
    }
}

void UIManager::ToggleOSD() {
    SetOSDVisible(!m_OSDVisible);
}

bool UIManager::IsOSDVisible() const {
    return m_OSDVisible;
}

void UIManager::ToggleOSDPanel(OSDPanel panel) {
    if (!m_InGameOSD) return;

    m_InGameOSD->TogglePanel(panel);

    const char *panelName = "Unknown";
    switch (panel) {
    case OSDPanel::Status:
        panelName = "Status";
        break;
    case OSDPanel::Velocity:
        panelName = "Velocity";
        break;
    case OSDPanel::Position:
        panelName = "Position";
        break;
    case OSDPanel::Physics:
        panelName = "Physics";
        break;
    case OSDPanel::Keys:
        panelName = "Keys";
        break;
    }

    bool isVisible = m_InGameOSD->IsPanelVisible(panel);
    if (m_Mod) {
        m_Mod->GetLogger()->Info("OSD %s panel %s", panelName, isVisible ? "shown" : "hidden");
    }
}

void UIManager::CycleTrajectoryPlane() {
    if (m_InGameOSD)
        m_InGameOSD->CycleTrajectoryPlane();
}

void UIManager::SetMode(UIMode mode) {
    if (m_CurrentMode == mode) return;

    m_CurrentMode = mode;

    const char *modeStr = "Unknown";
    switch (mode) {
    case UIMode::Idle: modeStr = "Idle";
        break;
    case UIMode::Playing: modeStr = "Playing";
        break;
    case UIMode::Paused: modeStr = "Paused";
        break;
    case UIMode::Recording: modeStr = "Recording";
        break;
    }

    if (m_Mod) {
        m_Mod->GetLogger()->Info("UI Mode changed to: %s", modeStr);
    }
}

void UIManager::UpdateHotkeys() {
    if (!m_Mod) return;

    // Handle stop key for both playback and recording
    if (m_Engine->IsPlaying() || m_Engine->IsRecording()) {
        if (ImGui::IsKeyPressed(m_StopHotkey)) {
            m_Engine->Stop();
            m_Mod->GetLogger()->Info("TAS stopped via stop hotkey.");
        }
    }

    // OSD hotkey handling using IsKeyToggled
    if (ImGui::IsKeyPressed(m_OSDHotkey)) {
        ToggleOSD();
    }

    // Panel-specific hotkeys (only when in-game and OSD is enabled)
    if (m_BML && m_BML->IsIngame() && m_OSDVisible) {
        if (ImGui::IsKeyPressed(m_StatusPanelHotkey)) {
            ToggleOSDPanel(OSDPanel::Status);
        }

        if (ImGui::IsKeyPressed(m_VelocityPanelHotkey)) {
            ToggleOSDPanel(OSDPanel::Velocity);
        }

        if (ImGui::IsKeyPressed(m_PositionPanelHotkey)) {
            ToggleOSDPanel(OSDPanel::Position);
        }

        if (ImGui::IsKeyPressed(m_PhysicsPanelHotkey)) {
            ToggleOSDPanel(OSDPanel::Physics);
        }

        if (ImGui::IsKeyPressed(m_KeysPanelHotkey)) {
            ToggleOSDPanel(OSDPanel::Keys);
        }

        if (ImGui::IsKeyPressed(m_TrajectoryPlaneHotkey)) {
            CycleTrajectoryPlane();
        }
    }
}