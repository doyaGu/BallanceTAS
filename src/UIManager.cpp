#include "UIManager.h"

#include "GameInterface.h"
#include "TASMenu.h"
#include "InGameOSD.h"
#include "TASEngine.h"

UIManager::UIManager(TASEngine *engine)
    : m_Engine(engine) {
    if (!m_Engine) {
        throw std::runtime_error("UIManager requires valid TASEngine.");
    }
}

UIManager::~UIManager() {
    Shutdown();
}

bool UIManager::Initialize() {
    if (m_Initialized) {
        m_Engine->GetLogger()->Warn("UIManager already initialized.");
        return true;
    }

    m_Engine->GetLogger()->Info("Initializing UIManager...");

    // Create and initialize TAS menu
    try {
        m_TASMenu = std::make_unique<TASMenu>(m_Engine);
        m_TASMenu->Init();
        m_Engine->GetLogger()->Info("TAS Menu initialized.");
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Failed to initialize TAS Menu: %s", e.what());
        return false;
    }

    // Create and initialize In-Game OSD
    try {
        m_InGameOSD = std::make_unique<InGameOSD>("TAS OSD", m_Engine);
        m_InGameOSD->SetVisibility(false); // Start hidden
        m_Engine->GetLogger()->Info("In-Game OSD initialized.");
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Failed to initialize In-Game OSD: %s", e.what());
        return false;
    }

    m_Initialized = true;
    m_Engine->GetLogger()->Info("UIManager initialized successfully.");
    return true;
}

void UIManager::Shutdown() {
    if (!m_Initialized) return;

    if (m_Engine && m_Engine->GetLogger()) {
        m_Engine->GetLogger()->Info("Shutting down UIManager...");
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

    if (m_Engine && m_Engine->GetLogger()) {
        m_Engine->GetLogger()->Info("UIManager shutdown complete.");
    }
}

void UIManager::Process() {
    if (!m_Initialized || !m_Engine || m_Engine->IsShuttingDown()) return;

    UpdateHotkeys();

    // Update OSD visibility based on game state
    if (m_InGameOSD) {
        bool shouldShowOSD = m_Engine->GetGameInterface()->IsIngame() && m_OSDVisible;
        m_InGameOSD->SetVisibility(shouldShowOSD);

        if (shouldShowOSD) {
            m_InGameOSD->Update(); // Allow OSD to update its data
        }
    }

    // Close TAS menu if we're in game (similar to MapMenu behavior)
    if (m_Engine->GetGameInterface()->IsIngame() && IsTASMenuOpen()) {
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
    if (!m_TASMenu || m_Engine->GetGameInterface()->IsIngame()) return;

    m_TASMenu->Open("TAS Projects");
    if (m_Engine) {
        m_Engine->GetLogger()->Info("TAS Menu opened.");
    }
}

void UIManager::CloseTASMenu() {
    if (!m_TASMenu) return;

    m_TASMenu->Close();
    if (m_Engine) {
        m_Engine->GetLogger()->Info("TAS Menu closed.");
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
    return m_TASMenu && m_TASMenu->IsOpen();
}

// === Recording Control ===

bool UIManager::StartRecording() {
    if (!m_Engine) return false;

    return m_Engine->StartRecording();
}

bool UIManager::StopRecording() {
    if (!m_Engine) return false;

    if (!m_Engine->IsRecording() && !m_Engine->IsPendingRecord()) {
        return false;
    }

    m_Engine->StopRecording();
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
    return m_Engine->StartReplay();
}

bool UIManager::StopReplay() {
    if (!m_Engine) return false;

    if (!m_Engine->IsPlaying() && !m_Engine->IsPendingPlay()) {
        return false;
    }

    m_Engine->StopReplay();
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

// === Translation Control ===

bool UIManager::StartTranslation() {
    if (!m_Engine) return false;

    return m_Engine->StartTranslation();
}

bool UIManager::StopTranslation() {
    if (!m_Engine) return false;

    if (!m_Engine->IsTranslating() && !m_Engine->IsPendingTranslate()) {
        return false;
    }

    m_Engine->StopTranslation();
    return true;
}

void UIManager::ToggleTranslation() {
    if (IsTranslating()) {
        StopTranslation();
    } else {
        StartTranslation();
    }
}

bool UIManager::IsTranslating() const {
    return m_Engine && (m_Engine->IsTranslating() || m_Engine->IsPendingTranslate());
}

// === OSD Control ===

void UIManager::SetOSDVisible(bool visible) {
    m_OSDVisible = visible;

    if (m_InGameOSD) {
        // Only actually show if we're in game
        bool shouldShow = visible && m_Engine->GetGameInterface()->IsIngame();
        m_InGameOSD->SetVisibility(shouldShow);
    }

    if (m_Engine) {
        m_Engine->GetLogger()->Info("OSD visibility set to %s", visible ? "visible" : "hidden");
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
    if (m_Engine) {
        m_Engine->GetLogger()->Info("OSD %s panel %s", panelName, isVisible ? "shown" : "hidden");
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

    if (m_Engine) {
        m_Engine->GetLogger()->Info("UI Mode changed to: %s", modeStr);
    }
}

void UIManager::UpdateHotkeys() {
    if (!m_Engine) return;

    // Handle stop key for playback, recording, and translation
    if (ImGui::IsKeyPressed(m_StopHotkey, false)) {
        if (m_Engine->IsTranslating()) {
            StopTranslation();
        } else if (m_Engine->IsPlaying()) {
            StopReplay();
        } else if (m_Engine->IsRecording()) {
            StopRecording();
        }
    }

    // OSD hotkey handling using IsKeyToggled
    if (ImGui::IsKeyPressed(m_OSDHotkey, false)) {
        ToggleOSD();
    }

    // Panel-specific hotkeys (only when in-game and OSD is enabled)
    if (m_Engine->GetGameInterface()->IsIngame() && m_OSDVisible) {
        if (ImGui::IsKeyPressed(m_StatusPanelHotkey, false)) {
            ToggleOSDPanel(OSDPanel::Status);
        }

        if (ImGui::IsKeyPressed(m_VelocityPanelHotkey, false)) {
            ToggleOSDPanel(OSDPanel::Velocity);
        }

        if (ImGui::IsKeyPressed(m_PositionPanelHotkey, false)) {
            ToggleOSDPanel(OSDPanel::Position);
        }

        if (ImGui::IsKeyPressed(m_PhysicsPanelHotkey, false)) {
            ToggleOSDPanel(OSDPanel::Physics);
        }

        if (ImGui::IsKeyPressed(m_KeysPanelHotkey, false)) {
            ToggleOSDPanel(OSDPanel::Keys);
        }

        if (ImGui::IsKeyPressed(m_TrajectoryPlaneHotkey, false)) {
            CycleTrajectoryPlane();
        }
    }
}
