#include "TASMenu.h"

#include "GameInterface.h"
#include "TASEngine.h"
#include "ProjectManager.h"
#include "ScriptGenerator.h"
#include "TASProject.h"
#include "UIManager.h"

// TASMenuPage Implementation
TASMenuPage::TASMenuPage(TASMenu *menu, std::string name) : Page(std::move(name)), m_Menu(menu) {
    if (m_Menu) {
        m_Menu->AddPage(this);
    }
}

TASMenuPage::~TASMenuPage() {
    if (m_Menu) {
        m_Menu->RemovePage(this);
    }
}

void TASMenuPage::OnClose() {
    if (m_Menu) {
        m_Menu->ShowPrevPage();
    }
}

// TASMenu Implementation
TASMenu::TASMenu(TASEngine *engine) : m_Engine(engine) {
    if (!m_Engine) {
        throw std::runtime_error("TASMenu requires valid TASEngine instances");
    }
}

TASMenu::~TASMenu() {
    Shutdown();
}

void TASMenu::Init() {
    try {
        m_TASListPage = std::make_unique<TASListPage>(this);
        m_TASDetailsPage = std::make_unique<TASDetailsPage>(this);
        m_TASRecordingPage = std::make_unique<TASRecordingPage>(this);

        RefreshProjects();
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Failed to initialize TAS Menu: %s", e.what());
        throw;
    }
}

void TASMenu::Shutdown() {
    try {
        // Reset pages in reverse order
        m_TASRecordingPage.reset();
        m_TASDetailsPage.reset();
        m_TASListPage.reset();
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Exception during TAS Menu shutdown: %s", e.what());
    }
}

bool TASMenu::IsOpen() const {
    return m_CurrentPage != nullptr && m_CurrentPage->IsVisible();
}

void TASMenu::OnOpen() {
    auto *inputManager = m_Engine->GetGameInterface()->GetInputManager();
    if (inputManager) {
        inputManager->Block(CK_INPUT_DEVICE_KEYBOARD);
    }

    if (!GetCurrentProject())
        RefreshProjects();
}

void TASMenu::OnClose() {
    m_Engine->GetGameInterface()->OnCloseMenu();
}

void TASMenu::RefreshProjects() {
    m_Engine->GetProjectManager()->RefreshProjects();
}

TASProject *TASMenu::GetCurrentProject() const {
    return m_Engine->GetProjectManager()->GetCurrentProject();
}

void TASMenu::SetCurrentProject(TASProject *project) {
    m_Engine->GetProjectManager()->SetCurrentProject(project);
}

void TASMenu::PlayProject(TASProject *project) {
    if (!project || !project->IsValid()) {
        m_Engine->GetLogger()->Error("Cannot play invalid project.");
        return;
    }

    // Stop any current TAS activity
    if (IsTASActive()) {
        StopTAS();
    }

    m_Engine->GetLogger()->Info("Playing TAS: %s", project->GetName().c_str());

    // Set the current project and start replay via TASEngine
    SetCurrentProject(project);

    if (m_Engine->StartReplay()) {
        Close(); // Close menu so user can load a level
    } else {
        m_Engine->GetLogger()->Error("Failed to start replay from menu.");
        // Reset project selection on failure
        SetCurrentProject(nullptr);
    }
}

void TASMenu::StopTAS(bool clearProject) {
    bool wasRecording = m_Engine->IsRecording() || m_Engine->IsPendingRecord();
    bool wasPlaying = m_Engine->IsPlaying() || m_Engine->IsPendingPlay();
    bool wasTranslating = m_Engine->IsTranslating() || m_Engine->IsPendingTranslate();

    if (wasTranslating) {
        m_Engine->StopTranslation(clearProject);
        m_Engine->GetLogger()->Info("Translation stopped from menu.");
    } else if (wasRecording) {
        m_Engine->StopRecording();
        m_Engine->GetLogger()->Info("Recording stopped from menu.");
    } else if (wasPlaying) {
        m_Engine->StopReplay(clearProject);
        m_Engine->GetLogger()->Info("Replay stopped from menu.");
    }

    if (clearProject) {
        // Clear project selection after stopping
        if (wasPlaying || wasTranslating) {
            SetCurrentProject(nullptr);
        }

        if (wasRecording || wasTranslating) {
            // Refresh projects (new one might have been generated)
            RefreshProjects();
        }
    }

    ShowPage("TAS Projects");
}

void TASMenu::StartRecording() {
    // Stop any current TAS activity
    if (IsTASActive()) {
        StopTAS();
    }

    // Clear any selected project since we're starting fresh
    SetCurrentProject(nullptr);

    if (m_Engine->StartRecording()) {
        m_Engine->GetLogger()->Info("Recording setup from menu.");
        Close(); // Close menu so user can load a level
    } else {
        m_Engine->GetLogger()->Error("Failed to setup recording from menu.");
    }
}

void TASMenu::StopRecording() {
    if (m_Engine->IsRecording() || m_Engine->IsPendingRecord()) {
        m_Engine->StopRecording();
        m_Engine->GetLogger()->Info("Recording stopped from menu.");

        // Refresh projects as a new one might have been generated
        RefreshProjects();
        ShowPage("TAS Projects");
    }
}

void TASMenu::TranslateProject(TASProject *project) {
    if (!project || !project->IsRecordProject() || !project->IsValid()) {
        m_Engine->GetLogger()->Error("Cannot translate: invalid record project.");
        return;
    }

    // Check if record can be accurately translated
    if (!project->CanBeTranslated()) {
        m_Engine->GetLogger()->Error("Cannot translate record: %s",
                                     project->GetTranslationCompatibilityMessage().c_str());
        return;
    }

    // Stop any current TAS activity
    if (IsTASActive()) {
        StopTAS();
    }

    m_Engine->GetLogger()->Info("Translating record to script: %s (%.1f Hz, constant timing)",
                                project->GetName().c_str(), project->GetUpdateRate());

    // Set the current project and start translation via TASEngine
    SetCurrentProject(project);

    if (m_Engine->StartTranslation()) {
        Close(); // Close menu so user can load a level
    } else {
        m_Engine->GetLogger()->Error("Failed to start translation from menu.");
        // Reset project selection on failure
        SetCurrentProject(nullptr);
    }
}

void TASMenu::StopTranslation() {
    if (m_Engine->IsTranslating() || m_Engine->IsPendingTranslate()) {
        m_Engine->StopTranslation();
        m_Engine->GetLogger()->Info("Translation stopped from menu.");

        // Refresh projects as a new script might have been generated
        RefreshProjects();
        ShowPage("TAS Projects");
    }
}

bool TASMenu::IsTASActive() const {
    return m_Engine->IsPlaying() || m_Engine->IsPendingPlay() ||
        m_Engine->IsRecording() || m_Engine->IsPendingRecord() ||
        m_Engine->IsTranslating() || m_Engine->IsPendingTranslate();
}

// TASListPage Implementation
TASListPage::TASListPage(TASMenu *menu) : TASMenuPage(menu, "TAS Projects") {}

TASListPage::~TASListPage() = default;

void TASListPage::OnAfterBegin() {
    if (!IsVisible() || !m_Menu || !m_Menu->GetEngine())
        return;

    DrawCenteredText(m_Title.c_str());

    auto *projectManager = m_Menu->GetEngine()->GetProjectManager();
    const auto &projects = projectManager->GetProjects();

    m_Count = static_cast<int>(projects.size());
    SetMaxPage(m_Count % MAX_ENTRIES_PER_PAGE == 0 ? m_Count / MAX_ENTRIES_PER_PAGE : m_Count / MAX_ENTRIES_PER_PAGE + 1);

    if (m_PageIndex > 0 &&
        LeftButton("PrevPage")) {
        PrevPage();
    }

    if (m_PageCount > 1 && m_PageIndex < m_PageCount - 1 &&
        RightButton("NextPage")) {
        NextPage();
    }
}

void TASListPage::OnDraw() {
    // Show TAS status if active
    if (m_Menu && m_Menu->IsTASActive()) {
        DrawTASStatus();
    }

    // Draw main action buttons
    DrawMainButtons();

    if (m_Count == 0) {
        // No projects message using standard positioning
        const auto menuPos = Bui::GetMenuPos();
        const auto menuSize = Bui::GetMenuSize();

        ImGui::SetCursorPosX(menuPos.x);
        ImGui::Dummy(Bui::CoordToPixel(ImVec2(0.375f, 0.2f)));

        ImGui::SetCursorPosX(menuPos.x);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        Page::WrappedText("No TAS projects found.", menuSize.x, 1.0f);
        ImGui::PopStyleColor();

        return;
    }

    bool v = true;
    const int n = GetPage() * MAX_ENTRIES_PER_PAGE;

    DrawEntries([&](std::size_t index) {
        return OnDrawEntry(n + index, &v);
    }, ImVec2(0.4031f, 0.24f), 0.06f, MAX_ENTRIES_PER_PAGE);
}

void TASListPage::DrawTASStatus() {
    if (!m_Menu || !m_Menu->GetEngine()) return;

    auto *engine = m_Menu->GetEngine();
    const auto menuPos = Bui::GetMenuPos();

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::Dummy(Bui::CoordToPixel(ImVec2(0.375f, 0.05f)));

    ImGui::SetCursorPosX(menuPos.x * 1.3f);

    if (engine->IsPlaying() || engine->IsPendingPlay()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
        ImGui::Text("[PLAYING]");
        ImGui::PopStyleColor();

        ImGui::SameLine();
        if (Bui::SmallButton("Stop")) {
            m_Menu->StopTAS();
        }
    } else if (engine->IsTranslating() || engine->IsPendingTranslate()) {
        // Animated effect for translation
        static float animTimer = 0.0f;
        animTimer += ImGui::GetIO().DeltaTime;
        float alpha = 0.6f + 0.4f * sin(animTimer * 3.0f); // Pulsing effect

        ImVec4 translateColor = ImVec4(0.2f, 0.8f, 1.0f, alpha);
        ImGui::PushStyleColor(ImGuiCol_Text, translateColor);
        ImGui::Text("[TRANSLATING]");
        ImGui::PopStyleColor();

        ImGui::SameLine();
        if (Bui::SmallButton("Stop")) {
            m_Menu->StopTranslation();
        }
    } else if (engine->IsRecording() || engine->IsPendingRecord()) {
        // Blinking effect for recording
        static float blinkTimer = 0.0f;
        blinkTimer += ImGui::GetIO().DeltaTime;
        bool blink = fmod(blinkTimer, 1.0f) < 0.5f;

        ImVec4 recordColor = blink ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) : ImVec4(0.8f, 0.4f, 0.4f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, recordColor);
        ImGui::Text("[RECORDING]");
        ImGui::PopStyleColor();

        ImGui::SameLine();
        if (Bui::SmallButton("Stop")) {
            m_Menu->StopRecording();
        }
    }

    ImGui::NewLine();
}

void TASListPage::DrawMainButtons() {
    if (!m_Menu || !m_Menu->GetEngine()) return;

    auto *engine = m_Menu->GetEngine();
    bool canRecord = engine && !engine->IsPlaying() && !engine->IsRecording() &&
        !engine->IsPendingPlay() && !engine->IsPendingRecord();

    // Record TAS button
    ImGui::SetCursorScreenPos(Bui::CoordToPixel(ImVec2(0.4031f, 0.18f)));

    if (!canRecord) {
        ImGui::BeginDisabled();
    }

    if (Bui::LevelButton("Record TAS")) {
        m_Menu->ShowPage("Record New TAS");
    }

    if (!canRecord) {
        ImGui::EndDisabled();
    }

    ImGui::NewLine();
}

bool TASListPage::OnDrawEntry(size_t index, bool *v) {
    if (!m_Menu) return false;

    auto *engine = m_Menu->GetEngine();
    if (!engine || !engine->GetProjectManager())
        return false;

    auto *projectManager = engine->GetProjectManager();
    const auto &projects = projectManager->GetProjects();

    if (index >= projects.size())
        return false;

    auto &project = projects[index];

    bool currentLegacyMode = engine->GetGameInterface()->IsLegacyMode();

    bool isCompatible = project->IsCompatibleWithSettings(currentLegacyMode);
    bool isInvalid = !project->IsValid();

    // Visual indicator for invalid projects
    if (isInvalid) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.2f, 0.6f));
    } else if (!isCompatible) {
        // Incompatible projects - orange/yellow
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.5f, 0.2f, 0.6f));
    } else if (project->IsRecordProject()) {
        // Record projects - purple/magenta
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.6f, 0.6f));
    } else if (project->IsZipProject()) {
        // Zip script projects - blue
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 0.6f));
    } else {
        // Regular script projects - default colors
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.6f));
    }

    const std::string &displayName = project->GetName();
    bool clicked = Bui::LevelButton(displayName.c_str(), v);

    ImGui::PopStyleColor(2);

    if (clicked) {
        m_Menu->SetCurrentProject(project.get());
        SetPage(0);
        m_Menu->ShowPage("TAS Details");
    }

    return true;
}

// TASDetailsPage Implementation
TASDetailsPage::TASDetailsPage(TASMenu *menu) : TASMenuPage(menu, "TAS Details") {}

TASDetailsPage::~TASDetailsPage() = default;

void TASDetailsPage::OnAfterBegin() {
    if (!IsVisible() || !m_Menu)
        return;

    auto *project = m_Menu->GetCurrentProject();
    if (!project) {
        m_Menu->ShowPrevPage();
        return;
    }

    const auto menuPos = Bui::GetMenuPos();
    const auto menuSize = Bui::GetMenuSize();

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::Dummy(Bui::CoordToPixel(ImVec2(0.375f, 0.05f)));

    ImGui::SetCursorPosX(menuPos.x);
    Page::WrappedText(project->GetName().c_str(), menuSize.x, 1.2f);
}

void TASDetailsPage::OnDraw() {
    if (!m_Menu) return;

    auto *project = m_Menu->GetCurrentProject();
    if (!project)
        return;

    DrawProjectInfo();
    DrawActionButtons();
}

void TASDetailsPage::DrawProjectInfo() {
    if (!m_Menu) return;

    auto *project = m_Menu->GetCurrentProject();
    if (!project) return;

    const auto menuPos = Bui::GetMenuPos();
    const auto menuSize = Bui::GetMenuSize();

    // Author
    if (!project->GetAuthor().empty()) {
        snprintf(m_TextBuf, sizeof(m_TextBuf), "By %s", project->GetAuthor().c_str());
        ImGui::SetCursorPosX(menuPos.x);
        Page::WrappedText(m_TextBuf, menuSize.x);
    }

    // Project type indicator
    ImGui::SetCursorPosX(menuPos.x);
    if (project->IsRecordProject()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 1.0f, 1.0f));
        Page::WrappedText("Type: Record (.tas)", menuSize.x);
        ImGui::PopStyleColor();
    } else if (project->IsZipProject()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 1.0f, 1.0f));
        Page::WrappedText("Type: Script (Archive)", menuSize.x);
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        Page::WrappedText("Type: Script (Directory)", menuSize.x);
        ImGui::PopStyleColor();
    }

    // Target Level
    if (!project->GetTargetLevel().empty()) {
        snprintf(m_TextBuf, sizeof(m_TextBuf), "Target Level: %s", project->GetTargetLevel().c_str());
        ImGui::SetCursorPosX(menuPos.x);
        Page::WrappedText(m_TextBuf, menuSize.x);
    }

    // Technical info
    snprintf(m_TextBuf, sizeof(m_TextBuf), "Update Rate: %.0f Hz", project->GetUpdateRate());
    ImGui::SetCursorPosX(menuPos.x);
    Page::WrappedText(m_TextBuf, menuSize.x);

    ImGui::NewLine();

    // Description
    ImGui::SetCursorPosX(menuPos.x);
    Page::WrappedText(project->GetDescription().c_str(), menuSize.x);

    // Get current BML settings for compatibility check
    bool currentLegacyMode = m_Menu->GetEngine()->GetGameInterface()->IsLegacyMode();
    bool isCompatible = project->IsCompatibleWithSettings(currentLegacyMode);

    // Compatibility status
    if (!isCompatible) {
        ImGui::NewLine();
        ImGui::SetCursorPosX(menuPos.x);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.6f, 1.0f));
        std::string compatMsg = project->GetCompatibilityMessage(currentLegacyMode);
        Page::WrappedText(compatMsg.c_str(), menuSize.x);
        ImGui::PopStyleColor();
    }

    // Validation status
    if (!project->IsValid()) {
        ImGui::NewLine();

        ImGui::SetCursorPosX(menuPos.x);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.6f, 1.0f));
        Page::WrappedText("Warning: This project has validation issues.", menuSize.x);
        ImGui::PopStyleColor();
    } else if (project->IsZipProject()) {
        ImGui::NewLine();

        ImGui::SetCursorPosX(menuPos.x);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
        Page::WrappedText("Note: Zip projects will be extracted to a temporary directory.", menuSize.x, 0.9f);
        ImGui::PopStyleColor();
    }

    // Special note for record projects
    if (project->IsRecordProject()) {
        ImGui::NewLine();

        ImGui::SetCursorPosX(menuPos.x);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.7f, 1.0f));
        Page::WrappedText("Note: Record projects (.tas files) can only be played in legacy mode.", menuSize.x, 0.9f);
        ImGui::PopStyleColor();
    }
}

void TASDetailsPage::DrawActionButtons() {
    if (!m_Menu || !m_Menu->GetEngine()) return;

    auto *project = m_Menu->GetCurrentProject();
    auto *engine = m_Menu->GetEngine();

    // Check current TAS state
    bool isTASActive = m_Menu->IsTASActive();
    bool canPlay = project->IsValid() && !isTASActive;
    bool canTranslate = project->CanBeTranslated() && !isTASActive;

    const auto menuPos = Bui::GetMenuPos();
    const auto menuSize = Bui::GetMenuSize();

    ImGui::NewLine();

    if (isTASActive) {
        // Show stop button if any TAS is active
        const ImVec2 buttonPos = Bui::CoordToPixel(ImVec2(0.35f, 0.0f));
        ImGui::SetCursorPosX(buttonPos.x);

        if (Bui::MainButton("Stop TAS")) {
            m_Menu->StopTAS();
        }

        ImGui::NewLine();

        ImGui::SetCursorPosX(menuPos.x);

        // Show status based on what's active
        if (engine->IsTranslating() || engine->IsPendingTranslate()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
            Page::WrappedText("Translation in progress", menuSize.x);
            ImGui::PopStyleColor();
        } else if (engine->IsPlaying() || engine->IsPendingPlay()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
            Page::WrappedText("TAS is currently playing", menuSize.x);
            ImGui::PopStyleColor();
        } else if (engine->IsRecording() || engine->IsPendingRecord()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
            Page::WrappedText("Recording in progress", menuSize.x);
            ImGui::PopStyleColor();
        }
    } else {
        // Show action buttons when idle
        if (project->IsRecordProject()) {
            // For record projects, show both Play and Translate options

            // Play button
            const ImVec2 playButtonPos = Bui::CoordToPixel(ImVec2(0.35f, 0.0f));
            ImGui::SetCursorPosX(playButtonPos.x);

            if (!canPlay) {
                ImGui::BeginDisabled();
            }

            if (Bui::MainButton("Play Record")) {
                m_Menu->PlayProject(project);
            }

            if (!canPlay) {
                ImGui::EndDisabled();
            }

            ImGui::NewLine();

            // Translate button (below play button)
            ImGui::SetCursorPosX(playButtonPos.x);

            if (!canTranslate) {
                ImGui::BeginDisabled();
            }

            if (Bui::MainButton("Translate to Script")) {
                m_Menu->TranslateProject(project);
            }

            if (!canTranslate) {
                ImGui::EndDisabled();
            }

            ImGui::NewLine();

            // Status information
            ImGui::SetCursorPosX(menuPos.x);

            if (!project->IsValid()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.7f, 1.0f));
                Page::WrappedText("Cannot use: Project has validation issues", menuSize.x);
                ImGui::PopStyleColor();
            } else if (!engine->GetGameInterface()->IsLegacyMode()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
                Page::WrappedText("Note: Record projects require legacy mode to be enabled", menuSize.x);
                ImGui::PopStyleColor();
            } else if (!project->CanBeTranslated()) {
                // Show translation compatibility issue
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.3f, 1.0f));
                std::string compatMsg = "Translation not recommended: " + project->GetTranslationCompatibilityMessage();
                Page::WrappedText(compatMsg.c_str(), menuSize.x);
                ImGui::PopStyleColor();

                ImGui::SetCursorPosX(menuPos.x * 1.05f);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                Page::WrappedText("Playing the record is still possible for viewing", menuSize.x, 0.9f);
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 1.0f, 0.7f, 1.0f));
                Page::WrappedText("Ready - Load a level after clicking an option", menuSize.x);
                ImGui::PopStyleColor();

                ImGui::SetCursorPosX(menuPos.x * 1.05f);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
                Page::WrappedText("Translate converts .tas records to script format", menuSize.x, 0.9f);
                ImGui::PopStyleColor();
            }
        } else {
            // For script projects, show only Play button (existing behavior)
            const ImVec2 buttonPos = Bui::CoordToPixel(ImVec2(0.35f, 0.0f));
            ImGui::SetCursorPosX(buttonPos.x);

            if (!canPlay) {
                ImGui::BeginDisabled();
            }

            if (Bui::MainButton("Play TAS")) {
                m_Menu->PlayProject(project);
            }

            if (!canPlay) {
                ImGui::EndDisabled();
            }

            ImGui::NewLine();

            // Show status information
            ImGui::SetCursorPosX(menuPos.x);

            if (!project->IsValid()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.7f, 1.0f));
                Page::WrappedText("Cannot play: Project has validation issues", menuSize.x);
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 1.0f, 0.7f, 1.0f));
                Page::WrappedText("Ready - Load a level after clicking Play TAS", menuSize.x);
                ImGui::PopStyleColor();
            }
        }
    }
}

// ===================================================================
// TASRecordingPage Implementation
// ===================================================================

TASRecordingPage::TASRecordingPage(TASMenu *menu) : TASMenuPage(menu, "Record New TAS") {
    if (!m_Menu || !m_Menu->GetEngine()) return;

    // Initialize with current level if available
    auto *engine = m_Menu->GetEngine();
    if (engine && engine->GetGameInterface()) {
        std::string currentMap = engine->GetGameInterface()->GetMapName();

        // Try to match current map to level options
        for (int i = 0; i < LEVEL_COUNT - 1; ++i) {
            if (currentMap.find(LEVEL_OPTIONS[i]) != std::string::npos ||
                currentMap == LEVEL_OPTIONS[i]) {
                m_TargetLevelIndex = i;
                break;
            }
        }
    }
}

void TASRecordingPage::OnAfterBegin() {
    if (!IsVisible()) return;

    DrawCenteredText("Record New TAS", 0.07f, 1.5f);
}

void TASRecordingPage::OnDraw() {
    if (!m_Menu) return;

    auto *engine = m_Menu->GetEngine();
    if (!engine) return;

    const auto menuPos = Bui::GetMenuPos();
    const auto menuSize = Bui::GetMenuSize();

    // Check if we can record
    bool canRecord = !engine->IsPlaying() && !engine->IsRecording() &&
        !engine->IsPendingPlay() && !engine->IsPendingRecord();

    if (!canRecord) {
        ImGui::SetCursorPosX(menuPos.x);
        ImGui::Dummy(Bui::CoordToPixel(ImVec2(0.375f, 0.1f)));

        ImGui::SetCursorPosX(menuPos.x);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.7f, 1.0f));

        if (engine->IsPlaying() || engine->IsPendingPlay()) {
            Page::WrappedText("Cannot record while TAS is playing.", menuSize.x);
        } else if (engine->IsRecording() || engine->IsPendingRecord()) {
            Page::WrappedText("Recording in progress or pending.", menuSize.x);
        }

        ImGui::PopStyleColor();
        return;
    }

    ImGui::Dummy(Bui::CoordToPixel(ImVec2(menuSize.x, 0.24f)));

    DrawRecordingControls();
    DrawGenerationOptions();

    ImGui::NewLine();

    DrawStartButton();
}

void TASRecordingPage::DrawRecordingControls() {
    if (!m_Menu) return;

    const auto menuPos = Bui::GetMenuPos();
    const auto menuSize = Bui::GetMenuSize();

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::Text("Project Settings:");

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::Text("Name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(menuSize.x * 0.6f);
    ImGui::InputText("##ProjectName", m_ProjectName, sizeof(m_ProjectName));

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::Text("Author:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(menuSize.x * 0.6f);
    ImGui::InputText("##AuthorName", m_AuthorName, sizeof(m_AuthorName));

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::Text("Level:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(menuSize.x * 0.4f);
    ImGui::Combo("##TargetLevel", &m_TargetLevelIndex, LEVEL_OPTIONS, LEVEL_COUNT);

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::Text("Update Rate:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(menuSize.x * 0.4f);
    ImGui::InputFloat("##UpdateRate", &m_UpdateRate);

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::Text("Description:");
    ImGui::SetCursorPosX(menuPos.x);
    ImGui::SetNextItemWidth(menuSize.x);
    ImGui::InputTextMultiline("##Description", m_Description, sizeof(m_Description),
                              ImVec2(menuSize.x, ImGui::GetTextLineHeight() * 3));
}

void TASRecordingPage::DrawGenerationOptions() {
    if (!m_Menu) return;

    const auto menuPos = Bui::GetMenuPos();
    const auto menuSize = Bui::GetMenuSize();

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::SetNextItemWidth(menuSize.x * 0.6f);
    ImGui::Checkbox("Add frame comments", &m_AddFrameComments);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Include frame numbers in generated comments");
    }

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::SetNextItemWidth(menuSize.x * 0.6f);
    ImGui::Checkbox("Advanced: Add physics comments", &m_AddPhysicsComments);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Include speed and physics data in comments (verbose)");
    }
}

void TASRecordingPage::DrawStartButton() {
    if (!m_Menu) return;

    const ImVec2 buttonPos = Bui::CoordToPixel(ImVec2(0.35f, 0.0f));

    ImGui::SetCursorPosX(buttonPos.x);
    if (Bui::MainButton("Start Recording")) {
        StartRecording();
    }
}

void TASRecordingPage::StartRecording() {
    if (!m_Menu) return;

    auto *engine = m_Menu->GetEngine();
    if (!engine) return;

    // Validate and clean project name
    std::string projectName = m_ProjectName;
    if (projectName.empty()) {
        projectName = "TAS_Untitled";
        strcpy_s(m_ProjectName, sizeof(m_ProjectName), projectName.c_str());
    }

    // Replace invalid characters
    std::replace_if(projectName.begin(), projectName.end(),
                    [](char c) {
                        return c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' ||
                            c == '?' || c == '"' || c == '<' || c == '>' || c == '|';
                    },
                    '_');

    // Update the field with cleaned name
    strcpy_s(m_ProjectName, sizeof(m_ProjectName), projectName.c_str());

    // Get target level string
    std::string targetLevel = "Level_01";
    if (m_TargetLevelIndex >= 0 && m_TargetLevelIndex < LEVEL_COUNT) {
        targetLevel = LEVEL_OPTIONS[m_TargetLevelIndex];
    }

    // Configure recorder with settings from UI
    if (auto *recorder = engine->GetRecorder()) {
        // Create generation options from UI settings
        GenerationOptions options;
        options.projectName = projectName;
        options.authorName = m_AuthorName;
        options.targetLevel = targetLevel;
        options.description = m_Description;
        options.updateRate = m_UpdateRate;
        options.addFrameComments = m_AddFrameComments;
        options.addPhysicsComments = m_AddPhysicsComments;

        // Set the generation options on the recorder
        recorder->SetGenerationOptions(options);
        recorder->SetUpdateRate(m_UpdateRate);
    }

    if (engine->StartRecording()) {
        engine->GetLogger()->Info("Recording setup for project: %s", projectName.c_str());
        engine->GetLogger()->Info("  Author: %s", m_AuthorName);
        engine->GetLogger()->Info("  Target Level: %s", targetLevel.c_str());
        engine->GetLogger()->Info("  Update Rate: %.3f Hz", m_UpdateRate);
        engine->GetLogger()->Info("  Description: %s", m_Description);
        engine->GetLogger()->Info("  Generation Options: frameComments=%s, physicsComments=%s",
                                  m_AddFrameComments ? "true" : "false",
                                  m_AddPhysicsComments ? "true" : "false");
        m_Menu->Close();
    } else {
        engine->GetLogger()->Error("Failed to setup recording.");
    }
}

void TASRecordingPage::StopRecording() {
    if (!m_Menu) return;

    auto *engine = m_Menu->GetEngine();
    if (!engine || (!engine->IsRecording() && !engine->IsPendingRecord())) return;

    engine->StopRecording();
    engine->GetLogger()->Info("Recording stopped from recording page.");
    m_Menu->ShowPrevPage();
}
