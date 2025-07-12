#ifndef BML_TASMENU_H
#define BML_TASMENU_H

#include <string>
#include <memory>

#include <BML/Bui.h>

class TASEngine;
class TASProject;
class TASMenu;

class TASMenuPage : public Bui::Page {
public:
    TASMenuPage(TASMenu *menu, std::string name);
    ~TASMenuPage() override;

    void OnClose() override;

protected:
    TASMenu *m_Menu;
};

class TASMenu : public Bui::Menu {
public:
    explicit TASMenu(TASEngine *engine);
    ~TASMenu() override;

    // TASMenu is not copyable or movable
    TASMenu(const TASMenu &) = delete;
    TASMenu &operator=(const TASMenu &) = delete;

    void Init();
    void Shutdown();

    void OnOpen() override;
    void OnClose() override;

    // Project management
    void RefreshProjects();
    TASProject *GetCurrentProject() const { return m_CurrentProject; }
    void SetCurrentProject(TASProject *project) { m_CurrentProject = project; }

    // Actions
    void PlayProject(TASProject *project);
    void StopTAS();
    void StartRecording();
    void StopRecording();
    void TranslateProject(TASProject *project);
    void StopTranslation();

    // State queries
    bool IsTASActive() const;
    bool IsOpen() const;

    TASEngine *GetEngine() const { return m_Engine; }

private:
    TASEngine *m_Engine;
    TASProject *m_CurrentProject = nullptr;

    std::unique_ptr<class TASListPage> m_TASListPage;
    std::unique_ptr<class TASDetailsPage> m_TASDetailsPage;
    std::unique_ptr<class TASRecordingPage> m_TASRecordingPage;
};

class TASListPage : public TASMenuPage {
public:
    explicit TASListPage(TASMenu *menu);
    ~TASListPage() override;

    void OnAfterBegin() override;
    void OnDraw() override;

private:
    bool OnDrawEntry(size_t index, bool *v);
    void DrawMainButtons();
    void DrawTASStatus();

    int m_Count = 0;
};

class TASDetailsPage : public TASMenuPage {
public:
    explicit TASDetailsPage(TASMenu *menu);
    ~TASDetailsPage() override;

    void OnAfterBegin() override;
    void OnDraw() override;

private:
    void DrawProjectInfo();
    void DrawActionButtons();

    char m_TextBuf[1024] = {};
};

/**
 * @class TASRecordingPage
 * @brief UI page for starting and configuring recordings.
 */
class TASRecordingPage : public TASMenuPage {
public:
    explicit TASRecordingPage(TASMenu *menu);
    ~TASRecordingPage() override = default;

    void OnAfterBegin() override;
    void OnDraw() override;

private:
    void DrawRecordingControls();
    void DrawGenerationOptions();
    void DrawStartButton();

    void StartRecording();
    void StopRecording();

    // Project configuration
    char m_ProjectName[256] = "TAS_Untitled";
    char m_AuthorName[128] = "Player";
    char m_Description[512] = "Recorded TAS run";
    int m_TargetLevelIndex = 0;
    float m_UpdateRate = 132.0f;

    // Generation options
    bool m_AddFrameComments = true;
    bool m_AddPhysicsComments = false;

    // Level selection constants
    static constexpr const char *LEVEL_OPTIONS[] = {
        "Level_01", "Level_02", "Level_03", "Level_04", "Level_05",
        "Level_06", "Level_07", "Level_08", "Level_09", "Level_10",
        "Level_11", "Level_12", "Level_13", "Custom"
    };
    static constexpr int LEVEL_COUNT = sizeof(LEVEL_OPTIONS) / sizeof(LEVEL_OPTIONS[0]);
};

#endif // BML_TASMENU_H