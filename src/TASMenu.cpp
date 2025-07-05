#include "TASMenu.h"

#include "BML/InputHook.h"

#include "BallanceTAS.h"
#include "GameInterface.h"
#include "TASEngine.h"
#include "ProjectManager.h"
#include "ScriptGenerator.h"
#include "TASProject.h"
#include "UIManager.h"

// TASMenuPage Implementation
TASMenuPage::TASMenuPage(TASMenu *menu, std::string name) : Page(std::move(name)), m_Menu(menu) {
    m_Menu->AddPage(this);
}

TASMenuPage::~TASMenuPage() {
    m_Menu->RemovePage(this);
}

void TASMenuPage::OnClose() {
    m_Menu->ShowPrevPage();
}

// TASMenu Implementation
TASMenu::TASMenu(TASEngine *engine) : m_Engine(engine), m_Mod(engine->GetMod()) {}

TASMenu::~TASMenu() {
    Shutdown();
}

void TASMenu::Init() {
    m_TASListPage = std::make_unique<TASListPage>(this);
    m_TASDetailsPage = std::make_unique<TASDetailsPage>(this);
    m_TASRecordingPage = std::make_unique<TASRecordingPage>(this);

    RefreshProjects();
}

void TASMenu::Shutdown() {
    m_TASRecordingPage.reset();
    m_TASDetailsPage.reset();
    m_TASListPage.reset();
}

void TASMenu::OnOpen() {
    m_Mod->GetInputManager()->Block(CK_INPUT_DEVICE_KEYBOARD);
    RefreshProjects();

    // No automatic page switching - let user navigate normally
}

void TASMenu::OnClose() {
    auto *bml = m_Mod->GetBML();
    auto *inputManager = m_Mod->GetInputManager();

    CKBehavior *beh = bml->GetScriptByName("Menu_Start");
    bml->GetCKContext()->GetCurrentScene()->Activate(beh, true);

    bml->AddTimerLoop(1ul, [inputManager] {
        if (inputManager->oIsKeyDown(CKKEY_ESCAPE) || inputManager->oIsKeyDown(CKKEY_RETURN))
            return true;
        inputManager->Unblock(CK_INPUT_DEVICE_KEYBOARD);
        return false;
    });

    m_CurrentProject = nullptr;
}

void TASMenu::RefreshProjects() {
    m_Engine->GetProjectManager()->RefreshProjects();
}

void TASMenu::PlayProject(TASProject *project) {
    if (!project || !project->IsValid()) return;

    m_Mod->GetLogger()->Info("Playing TAS: %s", project->GetName().c_str());

    // Load the TAS and close menu
    m_Engine->GetProjectManager()->SetCurrentProject(project);
    m_Engine->SetPlayPending(true);
    Close();
}

void TASMenu::StartRecording() {
    if (m_Engine->SetupRecording()) {
        m_Mod->GetLogger()->Info("Recording setup from menu.");
        Close(); // Close menu so user can load a level
    } else {
        m_Mod->GetLogger()->Error("Failed to setup recording from menu.");
    }
}

void TASMenu::StopRecording() {
    if (m_Engine->IsRecording()) {
        m_Engine->StopRecording();
        m_Mod->GetLogger()->Info("Recording stopped from menu.");
        ShowPage("TAS Projects");
    }
}

// TASListPage Implementation
TASListPage::TASListPage(TASMenu *menu) : TASMenuPage(menu, "TAS Projects") {}

TASListPage::~TASListPage() = default;

void TASListPage::OnAfterBegin() {
    if (!IsVisible())
        return;

    DrawCenteredText(m_Title.c_str(), 0.07f);

    auto *projectManager = m_Menu->GetEngine()->GetProjectManager();
    const auto &projects = projectManager->GetProjects();

    m_Count = static_cast<int>(projects.size());
    SetMaxPage(m_Count % 6 == 0 ? m_Count / 6 : m_Count / 6 + 1);

    // Navigation buttons
    if (m_PageIndex > 0 &&
        LeftButton("PrevPage", ImVec2(0.36f, 0.82f))) {
        PrevPage();
    }

    if (m_PageCount > 1 && m_PageIndex < m_PageCount - 1 &&
        RightButton("NextPage", ImVec2(0.6238f, 0.82f))) {
        NextPage();
    }
}

void TASListPage::OnDraw() {
    // Draw main action buttons first
    DrawMainButtons();

    if (m_Count == 0) {
        // Simple "no projects" message
        const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
        ImGui::SetCursorScreenPos(ImVec2(vpSize.x * 0.40f, vpSize.y * 0.5f));
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No projects found.");

        ImGui::SetCursorScreenPos(ImVec2(vpSize.x * 0.32f, vpSize.y * 0.55f));
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Record a new TAS to get started!");
        return;
    }

    bool v = true;
    const int n = GetPage() * 6;

    DrawEntries([&](std::size_t index) {
        return OnDrawEntry(n + index, &v);
    }, ImVec2(0.4031f, 0.25f), 0.06f, 6);
}

void TASListPage::DrawMainButtons() {
    auto *engine = m_Menu->GetEngine();
    bool canRecord = engine && !engine->IsPlaying() && !engine->IsRecording() && !engine->IsPendingPlay();

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

        // Show why recording is disabled
        ImGui::SetCursorScreenPos(Bui::CoordToPixel(ImVec2(0.2f, 0.23f)));
        if (engine->IsPlaying()) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "Stop current TAS to record");
        } else if (engine->IsRecording()) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "Already recording");
        } else if (engine->IsPendingRecord()) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.7f, 1.0f), "Recording pending - load level");
        }
    }
}

bool TASListPage::OnDrawEntry(size_t index, bool *v) {
    auto *projectManager = m_Menu->GetEngine()->GetProjectManager();
    const auto &projects = projectManager->GetProjects();

    if (index >= projects.size())
        return false;

    auto &project = projects[index];

    // Simple visual indicator for invalid projects
    if (!project->IsValid()) {
        ImGui::PushStyleColor(ImGuiCol_Text, 0xFF6666FF); // Light red
    }

    if (Bui::LevelButton(project->GetName().c_str(), v)) {
        m_Menu->SetCurrentProject(project.get());
        SetPage(0);
        m_Menu->ShowPage("TAS Details");
    }

    if (!project->IsValid()) {
        ImGui::PopStyleColor();
    }

    return true;
}

// TASDetailsPage Implementation
TASDetailsPage::TASDetailsPage(TASMenu *menu) : TASMenuPage(menu, "TAS Details") {}

TASDetailsPage::~TASDetailsPage() = default;

void TASDetailsPage::OnAfterBegin() {
    if (!IsVisible())
        return;

    auto *project = m_Menu->GetCurrentProject();
    if (!project) {
        m_Menu->ShowPrevPage();
        return;
    }

    DrawCenteredText(project->GetName().c_str(), 0.07f, 1.5f);
}

void TASDetailsPage::OnDraw() {
    auto *project = m_Menu->GetCurrentProject();
    if (!project)
        return;

    DrawProjectInfo();
    DrawActionButtons();
}

void TASDetailsPage::DrawProjectInfo() {
    auto *project = m_Menu->GetCurrentProject();
    const auto menuPos = Bui::GetMenuPos();
    const auto menuSize = Bui::GetMenuSize();

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::Dummy(Bui::CoordToPixel(ImVec2(0.375f, 0.15f)));

    // Author
    if (!project->GetAuthor().empty()) {
        snprintf(m_TextBuf, sizeof(m_TextBuf), "By %s", project->GetAuthor().c_str());
        ImGui::SetCursorPosX(menuPos.x);
        Page::WrappedText(m_TextBuf, menuSize.x);
        ImGui::NewLine();
    }

    // Target Level
    if (!project->GetTargetLevel().empty()) {
        snprintf(m_TextBuf, sizeof(m_TextBuf), "Target Level: %s", project->GetTargetLevel().c_str());
        ImGui::SetCursorPosX(menuPos.x);
        ImGui::Text("%s", m_TextBuf);
        ImGui::NewLine();
    }

    // Description
    if (!project->GetDescription().empty()) {
        ImGui::SetCursorPosX(menuPos.x);
        Page::WrappedText(project->GetDescription().c_str(), menuSize.x);
        ImGui::NewLine();
    }

    // Technical info
    ImGui::SetCursorPosX(menuPos.x);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Update Rate: %.0f Hz", project->GetUpdateRate());

    // Simple validity warning
    if (!project->IsValid()) {
        ImGui::SetCursorPosX(menuPos.x);
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "This project may have issues.");
    }
}

void TASDetailsPage::DrawActionButtons() {
    auto *project = m_Menu->GetCurrentProject();
    auto *engine = m_Menu->GetEngine();

    // Play button (centered)
    ImGui::SetCursorScreenPos(Bui::CoordToPixel(ImVec2(0.35f, 0.7f)));

    bool canPlay = engine && !engine->IsRecording() && !engine->IsPendingRecord() && project->IsValid();

    if (!canPlay) {
        ImGui::BeginDisabled();
    }

    if (Bui::MainButton("Play TAS")) {
        m_Menu->PlayProject(project);
    }

    if (!canPlay) {
        ImGui::EndDisabled();

        // Show why play is disabled
        if (engine && engine->IsRecording()) {
            ImGui::SetCursorScreenPos(Bui::CoordToPixel(ImVec2(0.3f, 0.75f)));
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "Stop recording to play TAS");
        } else if (engine && engine->IsPendingRecord()) {
            ImGui::SetCursorScreenPos(Bui::CoordToPixel(ImVec2(0.28f, 0.75f)));
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "Recording pending - cancel to play TAS");
        } else if (!project->IsValid()) {
            ImGui::SetCursorScreenPos(Bui::CoordToPixel(ImVec2(0.32f, 0.75f)));
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "Project has validation issues");
        }
    }
}

// ===================================================================
// TASRecordingPage Implementation
// ===================================================================

TASRecordingPage::TASRecordingPage(TASMenu *menu) : TASMenuPage(menu, "Record New TAS") {
    // Initialize with current level if available
    auto *engine = m_Menu->GetEngine();
    if (engine && engine->GetGameInterface()) {
        std::string currentMap = engine->GetGameInterface()->GetMapName();

        // Try to match current map to level options
        for (int i = 0; i < LEVEL_COUNT - 1; ++i) {
            // -1 to skip "Custom"
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

    auto *engine = m_Menu->GetEngine();
    if (!engine) {
        return;
    }

    // Check if we can record (not playing/already recording)
    if (engine->IsPlaying()) {
        ImGui::SetCursorScreenPos(Bui::CoordToPixel(ImVec2(0.42f, 0.2f)));
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "Cannot record while TAS is playing.");
        return;
    }

    if (engine->IsRecording()) {
        ImGui::SetCursorScreenPos(Bui::CoordToPixel(ImVec2(0.42f, 0.2f)));
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "In the middle of recording.");
    }
}

void TASRecordingPage::OnDraw() {
    auto *engine = m_Menu->GetEngine();
    if (!engine || engine->IsPlaying() || engine->IsRecording()) {
        return;
    }

    const auto menuPos = Bui::GetMenuPos();

    // Main content area
    ImGui::SetCursorPosX(menuPos.x);
    ImGui::Dummy(Bui::CoordToPixel(ImVec2(0.375f, 0.15f)));

    DrawRecordingControls();

    ImGui::Spacing();

    DrawGenerationOptions();

    ImGui::Spacing();

    // Help text
    ImGui::SetCursorPosX(menuPos.x);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    ImGui::Text("Start recording, then play normally.");
    ImGui::SetCursorPosX(menuPos.x);
    ImGui::Text("Press F3 to stop recording and generate script.");
    ImGui::PopStyleColor();

    // Start recording button
    ImGui::SetCursorScreenPos(Bui::CoordToPixel(ImVec2(0.35f, 0.75f)));
    if (Bui::MainButton("Start Recording")) {
        StartRecording();
    }
}

void TASRecordingPage::DrawRecordingControls() {
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
    ImGui::Text("Description:");
    ImGui::SetCursorPosX(menuPos.x);
    ImGui::SetNextItemWidth(menuSize.x);
    ImGui::InputTextMultiline("##Description", m_Description, sizeof(m_Description),
                              ImVec2(menuSize.x, ImGui::GetTextLineHeight() * 3));
}

void TASRecordingPage::DrawGenerationOptions() {
    const auto menuPos = Bui::GetMenuPos();
    const auto menuSize = Bui::GetMenuSize();

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::SetNextItemWidth(menuSize.x * 0.6f);
    ImGui::Checkbox("Optimize short waits", &m_OptimizeShortWaits);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Merge brief wait periods for cleaner code");
    }

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::SetNextItemWidth(menuSize.x * 0.6f);
    ImGui::Checkbox("Add frame comments", &m_AddFrameComments);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Include frame numbers in generated comments");
    }

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::SetNextItemWidth(menuSize.x * 0.6f);
    ImGui::Checkbox("Group similar actions", &m_GroupSimilarActions);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Combine similar input patterns");
    }

    ImGui::SetCursorPosX(menuPos.x);
    ImGui::SetNextItemWidth(menuSize.x * 0.6f);
    ImGui::Checkbox("Advanced: Add physics comments", &m_AddPhysicsComments);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Include speed and physics data in comments (verbose)");
    }
}

void TASRecordingPage::StartRecording() {
    auto *engine = m_Menu->GetEngine();
    if (!engine) return;

    // Validate project name
    std::string projectName = m_ProjectName;
    if (projectName.empty()) {
        projectName = "Untitled_Recording";
        strcpy_s(m_ProjectName, sizeof(m_ProjectName), projectName.c_str());
    }

    // Replace spaces and invalid characters
    std::replace_if(projectName.begin(), projectName.end(),
                    [](char c) {
                        return c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c ==
                            '<' || c == '>' || c == '|';
                    },
                    '_');

    if (engine->SetupRecording()) {
        engine->GetMod()->GetLogger()->Info("Recording setup for project: %s", projectName.c_str());
        // Close menu and wait for level load to start recording
        m_Menu->Close();
    } else {
        engine->GetMod()->GetLogger()->Error("Failed to setup recording.");
    }
}

void TASRecordingPage::StopRecording() {
    auto *engine = m_Menu->GetEngine();
    if (!engine || !engine->IsRecording()) return;

    std::string projectName = m_ProjectName;
    if (projectName.empty()) {
        projectName = "Generated_TAS";
    }

    // Create generation options
    GenerationOptions options;
    options.projectName = projectName;
    options.authorName = m_AuthorName;
    options.targetLevel = (m_TargetLevelIndex < LEVEL_COUNT - 1) ? LEVEL_OPTIONS[m_TargetLevelIndex] : "Custom";
    options.description = m_Description;
    options.optimizeShortWaits = m_OptimizeShortWaits;
    options.addFrameComments = m_AddFrameComments;
    options.addPhysicsComments = m_AddPhysicsComments;
    options.groupSimilarActions = m_GroupSimilarActions;

    bool success = engine->StopRecordingAndGenerate(projectName, &options);

    if (success) {
        engine->GetMod()->GetLogger()->Info("Recording completed and script generated.");
        m_Menu->ShowPrevPage(); // Go back to project list
    } else {
        engine->GetMod()->GetLogger()->Error("Failed to generate script from recording.");
    }
}