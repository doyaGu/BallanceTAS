#include "UIManager.h"

#include "BallanceTAS.h"
#include "TASMenu.h"
#include "InGameOSD.h"
#include "TASEngine.h"

#include <imgui.h>

UIManager::UIManager(TASEngine *engine)
    : m_Engine(engine), m_Mod(engine->GetMod()), m_BML(m_Mod->GetBML()) {}

UIManager::~UIManager() {
    Shutdown();
}

bool UIManager::Initialize() {
    if (m_Initialized) {
        m_Mod->GetLogger()->Warn("UIManager already initialized.");
        return true;
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

    m_Mod->GetLogger()->Info("Shutting down UIManager...");

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
    m_Mod->GetLogger()->Info("UIManager shutdown complete.");
}

void UIManager::Process() {
    if (!m_Initialized) return;

    UpdateHotkeys();

    // Update OSD visibility based on game state
    if (m_InGameOSD) {
        bool shouldShowOSD = m_BML->IsIngame() && m_OSDVisible;
        m_InGameOSD->SetVisibility(shouldShowOSD);

        if (shouldShowOSD) {
            m_InGameOSD->Update(); // Allow OSD to update its data
        }
    }

    // Close TAS menu if we're in game (similar to MapMenu behavior)
    if (m_BML->IsIngame() && IsTASMenuOpen()) {
        CloseTASMenu();
    }

    // Update recording prompt timer
    if (m_ShowRecordingPrompt) {
        m_RecordingPromptTimer += ImGui::GetIO().DeltaTime;
        if (m_RecordingPromptTimer > RECORDING_PROMPT_TIMEOUT) {
            m_ShowRecordingPrompt = false;
            m_RecordingPromptTimer = 0.0f;
        }
    }
}

void UIManager::Render() {
    if (!m_Initialized) return;

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

    // Render recording prompt overlay
    if (m_ShowRecordingPrompt) {
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                ImGuiWindowFlags_NoBackground |
                                ImGuiWindowFlags_NoMove |
                                ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_AlwaysAutoResize |
                                ImGuiWindowFlags_NoSavedSettings;

        const ImVec2& vpSize = ImGui::GetMainViewport()->Size;
        ImGui::SetNextWindowPos(ImVec2(vpSize.x * 0.5f, vpSize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        if (ImGui::Begin("Recording Prompt", nullptr, flags)) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.8f, 1.0f, 1.0f));

            if (ImGui::BeginChild("PromptContent", ImVec2(vpSize.x * 0.4f, vpSize.y * 0.3f), true)) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Recording Stopped!");
                ImGui::Separator();

                ImGui::Text("Generate TAS script?");
                ImGui::Spacing();

                ImGui::Text("Project Name:");
                ImGui::SetNextItemWidth(300);
                ImGui::InputText("##ProjectName", m_RecordingProjectName, sizeof(m_RecordingProjectName));

                ImGui::Spacing();

                float timeLeft = RECORDING_PROMPT_TIMEOUT - m_RecordingPromptTimer;
                ImGui::Text("Time remaining: %.1fs", timeLeft);

                ImGui::Spacing();

                if (ImGui::Button("Generate Script", ImVec2(120, 0))) {
                    std::string projectName = m_RecordingProjectName;
                    if (projectName.empty()) {
                        projectName = "Generated_TAS";
                    }

                    if (m_Engine->StopRecordingAndGenerate(projectName)) {
                        m_Mod->GetLogger()->Info("Script generated: %s", projectName.c_str());
                    }

                    m_ShowRecordingPrompt = false;
                    m_RecordingPromptTimer = 0.0f;
                }

                ImGui::SameLine();

                if (ImGui::Button("Discard", ImVec2(120, 0))) {
                    m_Engine->StopRecording();
                    m_ShowRecordingPrompt = false;
                    m_RecordingPromptTimer = 0.0f;
                }
            }
            ImGui::EndChild();

            ImGui::PopStyleColor(2);
        }
        ImGui::End();
    }
}

void UIManager::OpenTASMenu() {
    if (!m_TASMenu || m_BML->IsIngame()) return;

    m_TASMenu->Open("TAS Projects");
    m_Mod->GetLogger()->Info("TAS Menu opened.");
}

void UIManager::CloseTASMenu() {
    if (!m_TASMenu) return;

    m_TASMenu->Close();
    m_Mod->GetLogger()->Info("TAS Menu closed.");
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

bool UIManager::StartRecording() {
    if (!m_Engine) return false;

    if (m_Engine->IsPlaying()) {
        m_Mod->GetLogger()->Warn("Cannot start recording while TAS is playing.");
        return false;
    }

    if (m_Engine->IsRecording() || m_Engine->IsPendingRecord()) {
        m_Mod->GetLogger()->Warn("Already recording or recording is pending.");
        return false;
    }

    bool success = m_Engine->SetupRecording();
    if (success) {
        m_Mod->GetLogger()->Info("Recording setup complete. Load a level to begin recording.");
    }

    return success;
}

bool UIManager::StopRecording(const std::string& projectName) {
    if (!m_Engine || !m_Engine->IsRecording()) {
        return false;
    }

    if (!projectName.empty()) {
        // Generate script immediately with provided name
        return m_Engine->StopRecordingAndGenerate(projectName);
    } else {
        // Show prompt for project name
        m_ShowRecordingPrompt = true;
        m_RecordingPromptTimer = 0.0f;
        strcpy_s(m_RecordingProjectName, sizeof(m_RecordingProjectName), "Generated_TAS");
        return true;
    }
}

void UIManager::ToggleRecording() {
    if (IsRecording()) {
        StopRecording();
    } else if (m_Engine && m_Engine->IsPendingRecord()) {
        // Cancel pending recording
        m_Engine->Stop();
        m_Mod->GetLogger()->Info("Cancelled pending recording.");
    } else {
        StartRecording();
    }
}

bool UIManager::IsRecording() const {
    return m_Engine && (m_Engine->IsRecording() || m_Engine->IsPendingRecord());
}

void UIManager::SetOSDVisible(bool visible) {
    m_OSDVisible = visible;

    if (m_InGameOSD) {
        // Only actually show if we're in game
        bool shouldShow = visible && m_BML->IsIngame();
        m_InGameOSD->SetVisibility(shouldShow);
    }

    m_Mod->GetLogger()->Info("OSD visibility set to %s", visible ? "visible" : "hidden");
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
    m_Mod->GetLogger()->Info("OSD %s panel %s", panelName, isVisible ? "shown" : "hidden");
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

    m_Mod->GetLogger()->Info("UI Mode changed to: %s", modeStr);
}

void UIManager::UpdateHotkeys() {
    auto *inputManager = m_Mod->GetInputManager();

    // Recording hotkey handling
    if (inputManager->IsKeyToggled(m_RecordingHotkey)) {
        HandleRecordingHotkey();
    }

    // OSD hotkey handling using IsKeyToggled
    if (inputManager->IsKeyToggled(m_OSDHotkey)) {
        ToggleOSD();
    }

    // Panel-specific hotkeys (only when in-game and OSD is enabled)
    if (m_BML->IsIngame() && m_OSDVisible) {
        if (inputManager->IsKeyToggled(m_StatusPanelHotkey)) {
            ToggleOSDPanel(OSDPanel::Status);
        }

        if (inputManager->IsKeyToggled(m_VelocityPanelHotkey)) {
            ToggleOSDPanel(OSDPanel::Velocity);
        }

        if (inputManager->IsKeyToggled(m_PositionPanelHotkey)) {
            ToggleOSDPanel(OSDPanel::Position);
        }

        if (inputManager->IsKeyToggled(m_PhysicsPanelHotkey)) {
            ToggleOSDPanel(OSDPanel::Physics);
        }

        if (inputManager->IsKeyToggled(m_KeysPanelHotkey)) {
            ToggleOSDPanel(OSDPanel::Keys);
        }

        if (inputManager->IsKeyToggled(m_TrajectoryPlaneHotkey)) {
            CycleTrajectoryPlane();
        }
    }
}

void UIManager::HandleRecordingHotkey() {
    if (!m_Engine) return;

    if (m_Engine->IsRecording()) {
        // Stop recording and show prompt
        StopRecording();
    } else if (!m_Engine->IsPlaying()) {
        // Start recording if not playing
        StartRecording();
    } else {
        // Cannot record while playing
        m_Mod->GetLogger()->Warn("Cannot toggle recording while TAS is playing. Stop the TAS first.");
    }
}