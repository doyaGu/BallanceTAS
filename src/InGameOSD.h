#pragma once

#include <BML/Bui.h>
#include <string>
#include <vector>
#include <array>
#include <deque>

class TASEngine;
class BallanceTAS;
struct PhysicsObject;
class CKIpionManager;

/**
 * @enum OSDPanel
 * @brief Different information panels available in the OSD.
 */
enum class OSDPanel {
    Status,       // Basic TAS status and frame info
    Velocity,     // Real-time velocity graphs (X, Y, Z components + magnitude)
    Position,     // Position tracking and trajectory
    Physics,      // Angular velocity, mass, physics state
    Keys,         // Real-time key state display
};

/**
 * @enum TrajectoryPlane
 * @brief Different viewing planes for the trajectory graph.
 */
enum class TrajectoryPlane {
    XZ = 0,  // X-Z plane (default for Ballance)
    XY = 1,  // X-Y plane
    YZ = 2,  // Y-Z plane
    ZX = 3,  // Z-X plane (X and Z swapped)
    YX = 4,  // Y-X plane (X and Y swapped)
    ZY = 5,  // Z-Y plane (Y and Z swapped)
};

/**
 * @struct PhysicsHistory
 * @brief Stores historical physics data for graphing.
 */
struct PhysicsHistory {
    std::deque<float> velocityX;
    std::deque<float> velocityY;
    std::deque<float> velocityZ;
    std::deque<float> speed;
    std::deque<float> positionX;
    std::deque<float> positionY;
    std::deque<float> positionZ;
    std::deque<float> angularSpeed;
    std::deque<int> frameNumbers;

    static size_t s_MaxHistory;

    void AddFrame(const VxVector &velocity, const VxVector &position,
                  const VxVector &angularVel, int frame);
    void Clear();
};

/**
 * @class InGameOSD
 * @brief Advanced In-Game On-Screen Display with comprehensive physics visualization.
 *
 * Provides real-time graphical display of physics data, performance metrics,
 * and TAS information through interactive charts and visualizations.
 */
class InGameOSD : public Bui::Window {
public:
    InGameOSD(const std::string &name, TASEngine *engine);
    ~InGameOSD() override = default;

    // --- Window Overrides ---
    ImGuiWindowFlags GetFlags() override;
    void OnDraw() override;

    // --- Update Methods ---

    /**
     * @brief Updates all OSD data and physics history. Call this every frame when visible.
     */
    void Update();

    // --- Configuration API ---

    /**
     * @brief Toggles the visibility of a specific panel.
     * @param panel The panel to toggle.
     */
    void TogglePanel(OSDPanel panel);

    /**
     * @brief Sets the visibility of a specific panel.
     * @param panel The panel to configure.
     * @param visible Whether the panel should be visible.
     */
    void SetPanelVisible(OSDPanel panel, bool visible);

    /**
     * @brief Gets the visibility state of a specific panel.
     * @param panel The panel to check.
     * @return True if the panel is visible.
     */
    bool IsPanelVisible(OSDPanel panel) const;

    /**
     * @brief Sets the OSD position as screen percentage (0.0 to 1.0).
     * @param x Horizontal position (0.0 = left, 1.0 = right).
     * @param y Vertical position (0.0 = top, 1.0 = bottom).
     */
    void SetPosition(float x, float y);

    /**
     * @brief Sets the OSD opacity.
     * @param opacity Alpha value from 0.0 (transparent) to 1.0 (opaque).
     */
    void SetOpacity(float opacity) { m_Opacity = opacity; }

    /**
     * @brief Sets the scale factor for the OSD.
     * @param scale Scale multiplier (1.0 = normal size).
     */
    void SetScale(float scale) { m_Scale = scale; }

    /**
     * @brief Sets the time range for graphs in seconds.
     * @param seconds Time range to display (default: 5.0).
     */
    void SetGraphTimeRange(float seconds);

    /**
     * @brief Sets the viewing plane for the trajectory graph.
     * @param plane The plane to display (XZ, XY, or YZ).
     */
    void SetTrajectoryPlane(TrajectoryPlane plane) { m_TrajectoryPlane = plane; }

    /**
     * @brief Gets the current trajectory viewing plane.
     * @return The current trajectory plane.
     */
    TrajectoryPlane GetTrajectoryPlane() const { return m_TrajectoryPlane; }

    /**
     * @brief Cycles to the next trajectory viewing plane.
     */
    void CycleTrajectoryPlane();

    /**
     * @brief Clears all physics history data.
     */
    void ClearHistory() { m_PhysicsHistory.Clear(); }

private:
    // --- Rendering Methods ---
    void DrawStatusPanel();
    void DrawVelocityPanel();
    void DrawPositionPanel();
    void DrawPhysicsPanel();
    void DrawKeysPanel();
    void DrawPanelSeparator();

    // --- Graph Rendering Methods ---
    void DrawVelocityGraphs();
    void DrawPositionTrajectory();
    void DrawAngularVelocityIndicator();
    void DrawPhysicsStateIndicators();

    // --- Data Update Methods ---
    void UpdatePhysicsData();
    void UpdatePhysicsHistory();
    void UpdateKeyState();

    // --- Utility Methods ---
    PhysicsObject *GetBallPhysicsObject();
    ImVec4 GetVelocityColor(float velocity, float maxVel = 20.0f);
    float DrawKeyButton(ImDrawList *drawList, const ImVec2 &pos, float padding, float minHeight, bool pressed, const char *label);

    // Trajectory plane helpers
    void GetTrajectoryCoordinates(const VxVector &pos, float &coord1, float &coord2) const;
    std::pair<std::string, std::string> GetTrajectoryAxisLabels() const;
    void GetTrajectoryBounds(float &min1, float &max1, float &min2, float &max2) const;

    // --- Core References ---
    TASEngine *m_Engine;
    BallanceTAS *m_Mod;

    // --- Panel Visibility ---
    std::array<bool, 5> m_PanelVisible = {true, true, true, false, true}; // Status, Velocity, Position, Physics, Keys

    // --- Display Configuration ---
    float m_PosX = 0.02f;        // Screen position X (percentage)
    float m_PosY = 0.02f;        // Screen position Y (percentage)
    float m_Opacity = 0.9f;      // Window opacity
    float m_Scale = 1.0f;        // Text/UI scale factor
    float m_GraphTimeRange = 5.0f; // Graph time range in seconds

    // --- Trajectory Configuration ---
    TrajectoryPlane m_TrajectoryPlane = TrajectoryPlane::XZ;
    bool m_ShowTrajectoryControls = true;

    // --- Physics Data Cache ---
    struct PhysicsData {
        VxVector position = VxVector(0, 0, 0);
        VxVector velocity = VxVector(0, 0, 0);
        VxVector angularVelocity = VxVector(0, 0, 0);
        float speed = 0.0f;
        float angularSpeed = 0.0f;
        float mass = 0.0f;
        bool isValid = false;
    } m_PhysicsData;

    // --- Physics History for Graphs ---
    PhysicsHistory m_PhysicsHistory;

    // --- Frame Time History for Averaging ---
    std::vector<float> m_FrameTimeHistory;
    size_t m_FrameTimeHistorySize = 60; // 1 second at 60fps
    size_t m_FrameTimeIndex = 0;

    // --- Update Timing ---
    float m_LastUpdateTime = 0.0f;
    float m_UpdateInterval = 0.0166f; // Update data 60 times per second for smooth graphs

    // --- Graph Configuration ---
    ImVec2 m_SpeedGraphSize = ImVec2(280, 80);
    ImVec2 m_TrajectoryGraphSize = ImVec2(200, 200);
    ImVec2 m_MiniGraphSize = ImVec2(120, 40);

    // --- Key State Structure ---
    struct KeyState {
        bool keyUp = false;
        bool keyDown = false;
        bool keyLeft = false;
        bool keyRight = false;
        bool keyShift = false;
        bool keySpace = false;
        bool keyQ = false;
        bool keyEsc = false;
    } m_KeyState;
};