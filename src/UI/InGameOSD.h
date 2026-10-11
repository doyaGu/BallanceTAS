#pragma once

#include <array>
#include <deque>
#include <optional>
#include <string>

#include <BML/Bui.h>

class TASEngine;
struct PhysicsObject;

/**
 * @enum OSDPanel
 * @brief Information panels of the OSD.
 */
enum class OSDPanel {
    Status,     // TAS mode, playback frame, frame time, SR time, level state
    Ball,       // Position, velocity, per-frame velocity change
    Input,      // Key timeline around the current frame
    Validation, // Live comparison against the reference run
    Watches,    // Values scripts show with tas.hud.set
};

/**
 * @class InGameOSD
 * @brief In-game HUD for TAS work: everything it shows is read once per game frame.
 */
class InGameOSD : public Bui::Window {
public:
    InGameOSD(const std::string &name, TASEngine *engine);
    ~InGameOSD() override = default;

    // --- Window Overrides ---
    ImGuiWindowFlags GetFlags() override;
    void OnDraw() override;

    /**
     * @brief Reads this frame's state. Call once per game frame while the OSD is shown.
     */
    void Update();

    // --- Configuration API ---
    void TogglePanel(OSDPanel panel);
    void SetPanelVisible(OSDPanel panel, bool visible);
    bool IsPanelVisible(OSDPanel panel) const;

    /**
     * @brief Sets the OSD position as screen percentage (0.0 to 1.0).
     */
    void SetPosition(float x, float y);
    void SetOpacity(float opacity) { m_Opacity = opacity; }
    void SetScale(float scale) { m_Scale = scale; }

private:
    // Input timeline columns around this frame.
    static constexpr int kTimelinePast = 5;
    static constexpr int kTimelineFuture = 10;
    static constexpr size_t kKeyHistoryFrames = kTimelinePast + 1;

    void DrawStatusPanel();
    void DrawBallPanel();
    void DrawInputPanel();
    void DrawValidationPanel();
    void DrawWatchesPanel();

    void UpdateFrame();
    void UpdateBall();
    void UpdateInputHistory();
    int ReadGameKeys() const;
    PhysicsObject *GetBallPhysicsObject() const;

    TASEngine *m_Engine;

    std::array<bool, 5> m_PanelVisible = {true, true, true, true, true};

    float m_PosX = 0.02f;
    float m_PosY = 0.02f;
    float m_Opacity = 0.9f;
    float m_Scale = 1.0f;

    // --- This frame ---
    struct FrameInfo {
        const char *mode = "IDLE";
        ImVec4 modeColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        std::string project;
        std::optional<size_t> frame; // Playback frame just played, or recorded frame count - 1
        std::optional<size_t> total; // Record length during record playback
        bool fromRecord = false;     // Frames index the record being played
    } m_Frame;

    struct BallInfo {
        bool valid = false;
        VxVector position;
        VxVector velocity;
        VxVector angularVelocity;
        bool hasDelta = false;
        VxVector deltaVelocity; // Since the previous game frame
    } m_Ball;
    std::optional<VxVector> m_PreviousVelocity;

    // Keys the game saw in recent frames (RecordKeyState bits), oldest first; the last
    // entry is this frame.
    std::deque<int> m_KeyHistory;
};
