#include "InGameOSD.h"

#include <algorithm>

#include "TASEngine.h"
#include "GameInterface.h"
#include "UIManager.h"

size_t PhysicsHistory::s_MaxHistory = 300; // 5 seconds at 60fps

// PhysicsHistory Implementation
void PhysicsHistory::AddFrame(const VxVector &velocity, const VxVector &position,
                              const VxVector &angularVel, size_t frame) {
    // Add new data
    velocityX.push_back(velocity.x);
    velocityY.push_back(velocity.y);
    velocityZ.push_back(velocity.z);
    speed.push_back(velocity.Magnitude());
    positionX.push_back(position.x);
    positionY.push_back(position.y);
    positionZ.push_back(position.z);
    angularSpeed.push_back(angularVel.Magnitude());
    frameNumbers.push_back(frame);

    // Maintain maximum history size
    if (velocityX.size() > s_MaxHistory) {
        velocityX.pop_front();
        velocityY.pop_front();
        velocityZ.pop_front();
        speed.pop_front();
        positionX.pop_front();
        positionY.pop_front();
        positionZ.pop_front();
        angularSpeed.pop_front();
        frameNumbers.pop_front();
    }
}

void PhysicsHistory::Clear() {
    velocityX.clear();
    velocityY.clear();
    velocityZ.clear();
    speed.clear();
    positionX.clear();
    positionY.clear();
    positionZ.clear();
    angularSpeed.clear();
    frameNumbers.clear();
}

// InGameOSD Implementation
InGameOSD::InGameOSD(const std::string &name, TASEngine *engine)
    : Bui::Window(name), m_Engine(engine) {
    if (!m_Engine) {
        throw std::runtime_error("InGameOSD requires a valid TASEngine instance.");
    }

    // Initialize frame time history
    m_FrameTimeHistory.resize(m_FrameTimeHistorySize, 0.0f);

    // OSD should be hidden by default
    Hide();
}

ImGuiWindowFlags InGameOSD::GetFlags() {
    return ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize;
}

void InGameOSD::OnDraw() {
    // Set window position and scale
    const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
    ImGui::SetWindowPos(ImVec2(vpSize.x * m_PosX, vpSize.y * m_PosY));

    // Apply global opacity and scale
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_Opacity);

    const bool doScale = (m_Scale != 1.0f);

    if (doScale)
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * m_Scale);

    bool hasContent = false;

    // Draw enabled panels
    if (IsPanelVisible(OSDPanel::Status)) {
        DrawStatusPanel();
        hasContent = true;
    }

    if (IsPanelVisible(OSDPanel::Velocity)) {
        if (hasContent) DrawPanelSeparator();
        DrawVelocityPanel();
        hasContent = true;
    }

    if (IsPanelVisible(OSDPanel::Position)) {
        if (hasContent) DrawPanelSeparator();
        DrawPositionPanel();
        hasContent = true;
    }

    if (IsPanelVisible(OSDPanel::Physics)) {
        if (hasContent) DrawPanelSeparator();
        DrawPhysicsPanel();
        hasContent = true;
    }

    if (IsPanelVisible(OSDPanel::Keys)) {
        if (hasContent) DrawPanelSeparator();
        DrawKeysPanel();
        hasContent = true;
    }

    // Restore scale
    if (doScale)
        ImGui::PopFont();

    ImGui::PopStyleVar(); // Alpha
}

void InGameOSD::Update() {
    float currentTime = m_Engine->GetGameInterface()->GetTimeManager()->GetTime() / 1000.0f;

    // Update at 60fps for smooth graphs
    if (currentTime - m_LastUpdateTime < m_UpdateInterval) {
        return;
    }

    m_LastUpdateTime = currentTime;

    UpdatePhysicsData();
    UpdatePhysicsHistory();
    UpdateKeyState();
}

void InGameOSD::DrawStatusPanel() {
    // TAS Status with larger, more prominent display
    const char *modeText = "IDLE";
    ImVec4 modeColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

    auto *uiManager = m_Engine->GetGameInterface()->GetUIManager();
    if (uiManager) {
        switch (uiManager->GetMode()) {
        case UIMode::Playing:
            modeText = "PLAYING";
            modeColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
            break;
        case UIMode::Recording:
            modeText = "RECORDING";
            modeColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
            break;
        case UIMode::Paused:
            modeText = "PAUSED";
            modeColor = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            break;
        case UIMode::Idle:
        default:
            break;
        }
    }

    // Larger status text with blinking for recording
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));

    float oldScale = ImGui::GetFont()->Scale;
    ImGui::GetFont()->Scale *= 1.2f;
    ImGui::PushFont(ImGui::GetFont());

    // Add blinking effect for recording
    if (uiManager && uiManager->GetMode() == UIMode::Recording) {
        static float blinkTimer = 0.0f;
        blinkTimer += ImGui::GetIO().DeltaTime;
        bool blink = fmod(blinkTimer, 1.0f) < 0.5f;
        if (blink) {
            modeColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        }
    }

    ImGui::TextColored(modeColor, "[%s]", modeText);

    ImGui::GetFont()->Scale = oldScale;
    ImGui::PopFont();

    // Frame and level info
    ImGui::Text("Frame: %d", m_Engine->GetCurrentTick());

    ImGui::PopStyleVar();
}

void InGameOSD::DrawVelocityPanel() {
    if (!m_PhysicsData.isValid || m_PhysicsHistory.speed.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Velocity: No Data");
        return;
    }

    // Current velocity info
    ImGui::Text("Speed: %.3f", m_PhysicsData.speed);

    ImGui::TextColored(GetVelocityColor(m_PhysicsData.velocity.x), "X:%.3f", m_PhysicsData.velocity.x);
    ImGui::SameLine();
    ImGui::TextColored(GetVelocityColor(m_PhysicsData.velocity.y), "Y:%.3f", m_PhysicsData.velocity.y);
    ImGui::SameLine();
    ImGui::TextColored(GetVelocityColor(m_PhysicsData.velocity.z), "Z:%.3f", m_PhysicsData.velocity.z);

    DrawVelocityGraphs();
}

void InGameOSD::DrawPositionPanel() {
    if (!m_PhysicsData.isValid) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Position: No Data");
        return;
    }

    // Current position
    const auto &pos = m_PhysicsData.position;
    ImGui::Text("Pos: (%.3f, %.3f, %.3f)", pos.x, pos.y, pos.z);

    if (!m_PhysicsHistory.positionX.empty()) {
        DrawPositionTrajectory();
    }
}

void InGameOSD::DrawPhysicsPanel() {
    if (!m_PhysicsData.isValid) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Physics: No Data");
        return;
    }

    ImGui::TextColored(ImVec4(0.9f, 0.8f, 1.0f, 1.0f), "Physics State:");

    // Angular velocity
    ImGui::Text("Angular: %.3f", m_PhysicsData.angularSpeed);
    ImGui::SameLine();
    ImGui::Text("Mass: %.2f", m_PhysicsData.mass);

    DrawAngularVelocityIndicator();
    DrawPhysicsStateIndicators();
}

void InGameOSD::DrawKeysPanel() {
    // Get current window for drawing
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 cursorPos = ImGui::GetCursorPos();
    ImVec2 basePos = ImVec2(windowPos.x + cursorPos.x, windowPos.y + cursorPos.y);

    // Button styling - padding relative to font size
    ImVec2 fontSize = ImGui::CalcTextSize("M");    // Use 'M' as reference for font size
    float padding = fontSize.y * 0.4f * m_Scale;   // 40% of font height for padding
    float minHeight = fontSize.y * 1.8f * m_Scale; // 1.8x font height for button height
    float horizontalSpacing = 5.0f * m_Scale;
    float verticalSpacing = 5.0f * m_Scale;
    float arrowSpacing = 2.0f * m_Scale; // Tighter spacing for arrow keys

    // Row 1: ESC, Q
    float currentX = 0;
    float row1Y = basePos.y;

    currentX += DrawKeyButton(drawList, ImVec2(basePos.x + currentX, row1Y), padding, minHeight, m_KeyState.keyEsc, "ESC") + horizontalSpacing;
    currentX += DrawKeyButton(drawList, ImVec2(basePos.x + currentX, row1Y), padding, minHeight, m_KeyState.keyQ, "Q");

    // Row 2: Shift, Space
    currentX = 0;
    float row2Y = row1Y + minHeight + verticalSpacing;

    currentX += DrawKeyButton(drawList, ImVec2(basePos.x + currentX, row2Y), padding, minHeight, m_KeyState.keyShift, "Shift") + horizontalSpacing;
    currentX += DrawKeyButton(drawList, ImVec2(basePos.x + currentX, row2Y), padding, minHeight, m_KeyState.keySpace, "Space");

    // Arrow keys section - calculate positions for proper directional pad layout
    float arrowSectionY = row2Y + minHeight + verticalSpacing;

    // Pre-calculate arrow button sizes for centering
    float leftWidth = ImGui::CalcTextSize("<").x + padding * 2;
    float downWidth = ImGui::CalcTextSize("v").x + padding * 2;
    float rightWidth = ImGui::CalcTextSize(">").x + padding * 2;
    float upWidth = ImGui::CalcTextSize("^").x + padding * 2;

    // Calculate bottom row width (left + down + right + spacing)
    float bottomRowWidth = leftWidth + downWidth + rightWidth + arrowSpacing * 2;

    // Position up arrow centered above the bottom row
    float upButtonX = basePos.x + (bottomRowWidth - upWidth) * 0.5f;
    float upButtonY = arrowSectionY;
    DrawKeyButton(drawList, ImVec2(upButtonX, upButtonY), padding, minHeight, m_KeyState.keyUp, "^");

    // Bottom arrow row: left, down, right
    float bottomRowY = upButtonY + minHeight + arrowSpacing;
    float arrowX = basePos.x;

    arrowX += DrawKeyButton(drawList, ImVec2(arrowX, bottomRowY), padding, minHeight, m_KeyState.keyLeft, "<") + arrowSpacing;
    arrowX += DrawKeyButton(drawList, ImVec2(arrowX, bottomRowY), padding, minHeight, m_KeyState.keyDown, "v") + arrowSpacing;
    arrowX += DrawKeyButton(drawList, ImVec2(arrowX, bottomRowY), padding, minHeight, m_KeyState.keyRight, ">");

    // Calculate total width and height for dummy space
    float totalWidth = std::max({currentX, bottomRowWidth});
    float totalHeight = bottomRowY + minHeight - basePos.y;

    // Reserve space for the key display
    ImGui::Dummy(ImVec2(totalWidth, totalHeight));
}

void InGameOSD::DrawPanelSeparator() {
    ImGui::Spacing();
    ImGui::Spacing();
}

void InGameOSD::DrawVelocityGraphs() {
    if (m_PhysicsHistory.speed.size() < 2) return;

    // Convert deque to vector for ImGui plotting
    std::vector<float> speedData(m_PhysicsHistory.speed.begin(), m_PhysicsHistory.speed.end());
    std::vector<float> velXData(m_PhysicsHistory.velocityX.begin(), m_PhysicsHistory.velocityX.end());
    std::vector<float> velYData(m_PhysicsHistory.velocityY.begin(), m_PhysicsHistory.velocityY.end());
    std::vector<float> velZData(m_PhysicsHistory.velocityZ.begin(), m_PhysicsHistory.velocityZ.end());

    // Main speed graph
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
    ImGui::PlotLines("##Speed", speedData.data(), speedData.size(), 0, nullptr, 0.0f, 25.0f, m_SpeedGraphSize);
    ImGui::PopStyleColor();

    // Component velocity graphs (smaller)
    ImGui::Columns(3, "VelComponents", false);

    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    ImGui::PlotLines("##X", velXData.data(), velXData.size(), 0, nullptr, -15.0f, 15.0f, m_MiniGraphSize);
    ImGui::PopStyleColor();

    ImGui::NextColumn();
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
    ImGui::PlotLines("##Y", velYData.data(), velYData.size(), 0, nullptr, -15.0f, 15.0f, m_MiniGraphSize);
    ImGui::PopStyleColor();

    ImGui::NextColumn();
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.3f, 0.3f, 1.0f, 1.0f));
    ImGui::PlotLines("##Z", velZData.data(), velZData.size(), 0, nullptr, -15.0f, 15.0f, m_MiniGraphSize);
    ImGui::PopStyleColor();

    ImGui::Columns(1);
}

void InGameOSD::DrawPositionTrajectory() {
    if (m_PhysicsHistory.positionX.size() < 2) return;

    auto labels = GetTrajectoryAxisLabels();
    ImGui::Text("Trajectory (%s-%s):", labels.first.c_str(), labels.second.c_str());

    if (ImGui::BeginChild("Trajectory", ImVec2(m_TrajectoryGraphSize.x, m_TrajectoryGraphSize.y), true)) {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();

        if (canvasSize.x > 0 && canvasSize.y > 0 && !m_PhysicsHistory.positionX.empty()) {
            // Get bounds for the current viewing plane
            float min1, max1, min2, max2;
            GetTrajectoryBounds(min1, max1, min2, max2);

            float range1 = max1 - min1;
            float range2 = max2 - min2;

            if (range1 < 1.0f) range1 = 1.0f;
            if (range2 < 1.0f) range2 = 1.0f;

            // Draw trajectory line
            for (size_t i = 1; i < m_PhysicsHistory.positionX.size(); ++i) {
                float coord1_prev, coord2_prev, coord1_curr, coord2_curr;

                // Get coordinates for previous and current points
                VxVector prevPos(m_PhysicsHistory.positionX[i - 1],
                                 m_PhysicsHistory.positionY[i - 1],
                                 m_PhysicsHistory.positionZ[i - 1]);
                VxVector currPos(m_PhysicsHistory.positionX[i],
                                 m_PhysicsHistory.positionY[i],
                                 m_PhysicsHistory.positionZ[i]);

                GetTrajectoryCoordinates(prevPos, coord1_prev, coord2_prev);
                GetTrajectoryCoordinates(currPos, coord1_curr, coord2_curr);

                // Convert to screen space
                float x1 = (coord1_prev - min1) / range1 * canvasSize.x;
                float y1 = (coord2_prev - min2) / range2 * canvasSize.y;
                float x2 = (coord1_curr - min1) / range1 * canvasSize.x;
                float y2 = (coord2_curr - min2) / range2 * canvasSize.y;

                // Color fade from old to new
                float alpha = 0.3f + (0.7f * i / m_PhysicsHistory.positionX.size());
                ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.2f, 0.8f, 1.0f, alpha));

                drawList->AddLine(
                    ImVec2(canvasPos.x + x1, canvasPos.y + canvasSize.y - y1),
                    ImVec2(canvasPos.x + x2, canvasPos.y + canvasSize.y - y2),
                    color, 2.0f
                );
            }

            // Current position marker
            if (!m_PhysicsHistory.positionX.empty()) {
                VxVector currentPos(m_PhysicsHistory.positionX.back(),
                                    m_PhysicsHistory.positionY.back(),
                                    m_PhysicsHistory.positionZ.back());

                float currentCoord1, currentCoord2;
                GetTrajectoryCoordinates(currentPos, currentCoord1, currentCoord2);

                float currentX = (currentCoord1 - min1) / range1 * canvasSize.x;
                float currentY = (currentCoord2 - min2) / range2 * canvasSize.y;

                drawList->AddCircleFilled(
                    ImVec2(canvasPos.x + currentX, canvasPos.y + canvasSize.y - currentY),
                    4.0f,
                    ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 0.0f, 1.0f))
                );
            }
        }
    }
    ImGui::EndChild();
}

void InGameOSD::DrawAngularVelocityIndicator() {
    if (m_PhysicsData.angularSpeed < 0.1f) return;

    // Circular angular velocity indicator
    ImGui::Text("Rotation:");
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImVec2 center = ImGui::GetCursorScreenPos();
    center.x += 30;
    center.y += 30;

    float radius = 25.0f;
    float speed = std::min(m_PhysicsData.angularSpeed / 10.0f, 1.0f);

    // Background circle
    drawList->AddCircle(center, radius, ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.3f, 0.5f)), 16, 2.0f);

    // Speed indicator
    ImU32 speedColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.5f, 0.0f, speed));
    drawList->AddCircle(center, radius * speed, speedColor, 16, 3.0f);

    ImGui::Dummy(ImVec2(60, 60));
}

void InGameOSD::DrawPhysicsStateIndicators() {
    // Simple state indicators
    ImGui::Columns(2, "PhysicsState", false);

    // Velocity state
    ImVec4 velStateColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    const char *velState = "Static";

    if (m_PhysicsData.speed > 10.0f) {
        velStateColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        velState = "Fast";
    } else if (m_PhysicsData.speed > 3.0f) {
        velStateColor = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
        velState = "Medium";
    } else if (m_PhysicsData.speed > 0.5f) {
        velStateColor = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        velState = "Slow";
    }

    ImGui::TextColored(velStateColor, "State: %s", velState);

    ImGui::NextColumn();

    // Angular state
    ImVec4 angStateColor = m_PhysicsData.angularSpeed > 1.0f
                               ? ImVec4(1.0f, 0.5f, 0.0f, 1.0f)
                               : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    ImGui::TextColored(angStateColor, "Spin: %.1f", m_PhysicsData.angularSpeed);

    ImGui::Columns(1);
}

void InGameOSD::UpdatePhysicsData() {
    m_PhysicsData.isValid = false;

    auto *ball = GetBallPhysicsObject();
    if (!ball) return;

    try {
        // Get position and velocity
        VxVector position, angles, velocity, angularVelocity;
        ball->GetPosition(&position, &angles);
        ball->GetVelocity(&velocity, &angularVelocity);

        m_PhysicsData.position = position;
        m_PhysicsData.velocity = velocity;
        m_PhysicsData.angularVelocity = angularVelocity;
        m_PhysicsData.speed = velocity.Magnitude();
        m_PhysicsData.angularSpeed = angularVelocity.Magnitude();
        m_PhysicsData.mass = ball->GetMass();
        m_PhysicsData.isValid = true;
    } catch (...) {
        m_PhysicsData.isValid = false;
    }
}

void InGameOSD::UpdatePhysicsHistory() {
    if (!m_PhysicsData.isValid) return;

    size_t currentFrame = 0;
    if (m_Engine) {
        currentFrame = m_Engine->GetCurrentTick();
    }

    m_PhysicsHistory.AddFrame(
        m_PhysicsData.velocity,
        m_PhysicsData.position,
        m_PhysicsData.angularVelocity,
        currentFrame
    );
}

void InGameOSD::UpdateKeyState() {
    if (!m_Engine) return;

    auto *inputManager = m_Engine->GetGameInterface()->GetInputManager();

    // Update key states based on current input
    m_KeyState.keyUp = inputManager->IsKeyDown(CKKEY_UP);
    m_KeyState.keyDown = inputManager->IsKeyDown(CKKEY_DOWN);
    m_KeyState.keyLeft = inputManager->IsKeyDown(CKKEY_LEFT);
    m_KeyState.keyRight = inputManager->IsKeyDown(CKKEY_RIGHT);
    m_KeyState.keyShift = inputManager->IsKeyDown(CKKEY_LSHIFT);
    m_KeyState.keySpace = inputManager->IsKeyDown(CKKEY_SPACE);
    m_KeyState.keyQ = inputManager->IsKeyDown(CKKEY_Q);
    m_KeyState.keyEsc = inputManager->IsKeyDown(CKKEY_ESCAPE);
}

PhysicsObject *InGameOSD::GetBallPhysicsObject() {
    auto *context = m_Engine->GetGameInterface()->GetCKContext();
    if (!context) return nullptr;

    auto *gameInterface = m_Engine->GetGameInterface();
    if (!gameInterface) return nullptr;

    auto *ball = gameInterface->GetActiveBall();
    if (ball) {
        auto *physicsObj = gameInterface->GetPhysicsObject(ball);
        if (physicsObj) {
            return physicsObj;
        }
    }

    return nullptr;
}

ImVec4 InGameOSD::GetVelocityColor(float velocity, float maxVel) {
    float absVel = std::abs(velocity);
    float ratio = std::min(absVel / maxVel, 1.0f);

    if (ratio < 0.3f) {
        return ImVec4(0.3f, 1.0f, 0.3f, 1.0f); // Green for low
    } else if (ratio < 0.7f) {
        return ImVec4(1.0f, 0.8f, 0.3f, 1.0f); // Yellow for medium
    } else {
        return ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // Red for high
    }
}

float InGameOSD::DrawKeyButton(ImDrawList *drawList, const ImVec2 &pos, float padding, float minHeight, bool pressed, const char *label) {
    // Calculate text size once
    ImVec2 textSize = ImGui::CalcTextSize(label);

    // Calculate button size
    ImVec2 buttonSize = ImVec2(
        (textSize.x + padding * 2) * m_Scale,
        std::max(textSize.y + padding * 2, minHeight) * m_Scale
    );

    // Colors for pressed/unpressed states
    ImU32 bgColor = pressed ?
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.2f, 0.8f, 0.2f, 0.8f)) :  // Green when pressed
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.3f, 0.6f));   // Gray when not pressed

    ImU32 borderColor = pressed ?
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 1.0f, 0.4f, 1.0f)) :  // Bright green border when pressed
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.6f, 0.6f, 0.8f));   // Light gray border when not pressed

    ImU32 textColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // White text

    // Draw button background
    drawList->AddRectFilled(pos, ImVec2(pos.x + buttonSize.x, pos.y + buttonSize.y), bgColor, 3.0f);

    // Draw button border
    drawList->AddRect(pos, ImVec2(pos.x + buttonSize.x, pos.y + buttonSize.y), borderColor, 3.0f, 0, 2.0f);

    // Draw text label (centered) - reuse textSize from above
    ImVec2 textPos = ImVec2(
        pos.x + (buttonSize.x - textSize.x) * 0.5f,
        pos.y + (buttonSize.y - textSize.y) * 0.5f
    );

    drawList->AddText(textPos, textColor, label);

    // Return the width of the button for positioning next button
    return buttonSize.x;
}

bool InGameOSD::IsPanelVisible(OSDPanel panel) const {
    size_t index = static_cast<size_t>(panel);
    return index < m_PanelVisible.size() ? m_PanelVisible[index] : false;
}

void InGameOSD::TogglePanel(OSDPanel panel) {
    SetPanelVisible(panel, !IsPanelVisible(panel));
}

void InGameOSD::SetPanelVisible(OSDPanel panel, bool visible) {
    size_t index = static_cast<size_t>(panel);
    if (index < m_PanelVisible.size()) {
        m_PanelVisible[index] = visible;
    }
}

void InGameOSD::SetPosition(float x, float y) {
    m_PosX = std::max(0.0f, std::min(1.0f, x));
    m_PosY = std::max(0.0f, std::min(1.0f, y));
}

void InGameOSD::SetGraphTimeRange(float seconds) {
    m_GraphTimeRange = std::max(1.0f, std::min(30.0f, seconds));
    // Update max history based on time range
    size_t newMaxHistory = static_cast<size_t>(m_GraphTimeRange * 60.0f); // 60 FPS
    PhysicsHistory::s_MaxHistory = newMaxHistory;
}

void InGameOSD::CycleTrajectoryPlane() {
    switch (m_TrajectoryPlane) {
    case TrajectoryPlane::XZ:
        m_TrajectoryPlane = TrajectoryPlane::XY;
        break;
    case TrajectoryPlane::XY:
        m_TrajectoryPlane = TrajectoryPlane::YZ;
        break;
    case TrajectoryPlane::YZ:
        m_TrajectoryPlane = TrajectoryPlane::ZX;
        break;
    case TrajectoryPlane::ZX:
        m_TrajectoryPlane = TrajectoryPlane::YX;
        break;
    case TrajectoryPlane::YX:
        m_TrajectoryPlane = TrajectoryPlane::ZY;
        break;
    case TrajectoryPlane::ZY:
        m_TrajectoryPlane = TrajectoryPlane::XZ;
        break;
    }
}

void InGameOSD::GetTrajectoryCoordinates(const VxVector &pos, float &coord1, float &coord2) const {
    switch (m_TrajectoryPlane) {
    case TrajectoryPlane::XZ:
        coord1 = pos.x;
        coord2 = pos.z;
        break;
    case TrajectoryPlane::XY:
        coord1 = pos.x;
        coord2 = pos.y;
        break;
    case TrajectoryPlane::YZ:
        coord1 = pos.y;
        coord2 = pos.z;
        break;
    case TrajectoryPlane::ZX:
        coord1 = pos.z;
        coord2 = pos.x;
        break;
    case TrajectoryPlane::YX:
        coord1 = pos.y;
        coord2 = pos.x;
        break;
    case TrajectoryPlane::ZY:
        coord1 = pos.z;
        coord2 = pos.y;
        break;
    }
}

std::pair<std::string, std::string> InGameOSD::GetTrajectoryAxisLabels() const {
    switch (m_TrajectoryPlane) {
    case TrajectoryPlane::XZ:
        return {"X", "Z"};
    case TrajectoryPlane::XY:
        return {"X", "Y"};
    case TrajectoryPlane::YZ:
        return {"Y", "Z"};
    case TrajectoryPlane::ZX:
        return {"Z", "X"};
    case TrajectoryPlane::YX:
        return {"Y", "X"};
    case TrajectoryPlane::ZY:
        return {"Z", "Y"};
    default:
        return {"X", "Z"};
    }
}

void InGameOSD::GetTrajectoryBounds(float &min1, float &max1, float &min2, float &max2) const {
    if (m_PhysicsHistory.positionX.empty()) {
        min1 = max1 = min2 = max2 = 0.0f;
        return;
    }

    switch (m_TrajectoryPlane) {
    case TrajectoryPlane::XZ: {
        auto minMaxX = std::minmax_element(m_PhysicsHistory.positionX.begin(), m_PhysicsHistory.positionX.end());
        auto minMaxZ = std::minmax_element(m_PhysicsHistory.positionZ.begin(), m_PhysicsHistory.positionZ.end());
        min1 = *minMaxX.first;
        max1 = *minMaxX.second;
        min2 = *minMaxZ.first;
        max2 = *minMaxZ.second;
        break;
    }
    case TrajectoryPlane::XY: {
        auto minMaxX = std::minmax_element(m_PhysicsHistory.positionX.begin(), m_PhysicsHistory.positionX.end());
        auto minMaxY = std::minmax_element(m_PhysicsHistory.positionY.begin(), m_PhysicsHistory.positionY.end());
        min1 = *minMaxX.first;
        max1 = *minMaxX.second;
        min2 = *minMaxY.first;
        max2 = *minMaxY.second;
        break;
    }
    case TrajectoryPlane::YZ: {
        auto minMaxY = std::minmax_element(m_PhysicsHistory.positionY.begin(), m_PhysicsHistory.positionY.end());
        auto minMaxZ = std::minmax_element(m_PhysicsHistory.positionZ.begin(), m_PhysicsHistory.positionZ.end());
        min1 = *minMaxY.first;
        max1 = *minMaxY.second;
        min2 = *minMaxZ.first;
        max2 = *minMaxZ.second;
        break;
    }
    case TrajectoryPlane::ZX: {
        auto minMaxZ = std::minmax_element(m_PhysicsHistory.positionZ.begin(), m_PhysicsHistory.positionZ.end());
        auto minMaxX = std::minmax_element(m_PhysicsHistory.positionX.begin(), m_PhysicsHistory.positionX.end());
        min1 = *minMaxZ.first;
        max1 = *minMaxZ.second;
        min2 = *minMaxX.first;
        max2 = *minMaxX.second;
        break;
    }
    case TrajectoryPlane::YX: {
        auto minMaxY = std::minmax_element(m_PhysicsHistory.positionY.begin(), m_PhysicsHistory.positionY.end());
        auto minMaxX = std::minmax_element(m_PhysicsHistory.positionX.begin(), m_PhysicsHistory.positionX.end());
        min1 = *minMaxY.first;
        max1 = *minMaxY.second;
        min2 = *minMaxX.first;
        max2 = *minMaxX.second;
        break;
    }
    case TrajectoryPlane::ZY: {
        auto minMaxZ = std::minmax_element(m_PhysicsHistory.positionZ.begin(), m_PhysicsHistory.positionZ.end());
        auto minMaxY = std::minmax_element(m_PhysicsHistory.positionY.begin(), m_PhysicsHistory.positionY.end());
        min1 = *minMaxZ.first;
        max1 = *minMaxZ.second;
        min2 = *minMaxY.first;
        max2 = *minMaxY.second;
        break;
    }
    }
}
