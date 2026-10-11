#include "InGameOSD.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <stdexcept>

#include "GameInterface.h"
#include "HudWatches.h"
#include "PlaybackService.h"
#include "ProjectManager.h"
#include "RecordPlayer.h"
#include "TASEngine.h"
#include "TASProject.h"
#include "ValidationService.h"

namespace {
const ImVec4 kDim(0.65f, 0.65f, 0.65f, 1.0f);
const ImVec4 kGood(0.3f, 1.0f, 0.3f, 1.0f);
const ImVec4 kBad(1.0f, 0.35f, 0.35f, 1.0f);
const ImVec4 kNote(1.0f, 0.8f, 0.3f, 1.0f);

// Timeline rows: label and RecordKeyState bit.
struct KeyRow {
    const char *label;
    int bit;
};
constexpr KeyRow kKeyRows[] = {
    {"Up", 0}, {"Dn", 1}, {"Lt", 2}, {"Rt", 3}, {"Sh", 4}, {"Sp", 5}, {"Q", 6},
};
} // namespace

InGameOSD::InGameOSD(const std::string &name, TASEngine *engine)
    : Bui::Window(name), m_Engine(engine) {
    if (!m_Engine) {
        throw std::runtime_error("InGameOSD requires a valid TASEngine instance.");
    }
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
    const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
    ImGui::SetWindowPos(ImVec2(vpSize.x * m_PosX, vpSize.y * m_PosY));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_Opacity);
    const bool doScale = m_Scale != 1.0f;
    if (doScale) {
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * m_Scale);
    }

    bool first = true;
    auto section = [&](OSDPanel panel, void (InGameOSD::*draw)()) {
        if (!IsPanelVisible(panel)) {
            return;
        }
        if (!first) {
            ImGui::Spacing();
        }
        first = false;
        (this->*draw)();
    };
    section(OSDPanel::Status, &InGameOSD::DrawStatusPanel);
    section(OSDPanel::Ball, &InGameOSD::DrawBallPanel);
    section(OSDPanel::Input, &InGameOSD::DrawInputPanel);
    // Shown only while there is something to show.
    if (auto *validation = m_Engine->GetServiceProvider().Resolve<ValidationService>();
        validation && validation->IsActive()) {
        section(OSDPanel::Validation, &InGameOSD::DrawValidationPanel);
    }
    if (auto *watches = m_Engine->GetServiceProvider().Resolve<HudWatches>();
        watches && !watches->Entries().empty()) {
        section(OSDPanel::Watches, &InGameOSD::DrawWatchesPanel);
    }

    if (doScale) {
        ImGui::PopFont();
    }
    ImGui::PopStyleVar();
}

void InGameOSD::Update() {
    // A paused level does not advance; keep showing the frame it stopped at.
    if (m_Engine->GetGameInterface()->IsPaused()) {
        return;
    }
    UpdateFrame();
    UpdateBall();
    UpdateInputHistory();
}

// --- Data ---

void InGameOSD::UpdateFrame() {
    FrameInfo info;
    auto &services = m_Engine->GetServiceProvider();
    auto *playback = services.Resolve<PlaybackService>();
    auto *records = services.Resolve<RecordPlayer>();

    if (m_Engine->IsRecording()) {
        info.mode = "RECORDING";
        info.modeColor = kBad;
        const size_t frames = m_Engine->GetRecordingFrameCount();
        if (frames > 0) {
            info.frame = frames - 1;
        }
    } else if (m_Engine->IsPaused()) {
        info.mode = "PAUSED";
        info.modeColor = kNote;
    } else if (m_Engine->IsTranslating()) {
        info.mode = "TRANSLATING";
        info.modeColor = ImVec4(0.4f, 0.85f, 1.0f, 1.0f);
    } else if (m_Engine->IsPlaying()) {
        info.mode = "PLAYING";
        info.modeColor = kGood;
    } else if (m_Engine->IsPendingPlay() || m_Engine->IsPendingRecord() || m_Engine->IsPendingTranslate()) {
        info.mode = "ARMED";
        info.modeColor = kNote;
    }

    if (records && records->IsPlaying() && records->GetCurrentFrame() > 0) {
        info.frame = records->GetCurrentFrame() - 1;
        info.total = records->GetTotalFrames();
        info.fromRecord = true;
    } else if (playback && playback->IsPlaying() && playback->GetCurrentTick() > 0) {
        info.frame = playback->GetCurrentTick() - 1;
    }

    TASProject *project = playback && playback->IsPlaying() ? playback->GetCurrentProject() : nullptr;
    if (!project && (m_Engine->IsPendingPlay() || m_Engine->IsPendingTranslate() || m_Engine->IsTranslating())) {
        if (auto *projects = services.Resolve<ProjectManager>()) {
            project = projects->GetCurrentProject();
        }
    }
    if (project) {
        info.project = project->GetName();
    }
    m_Frame = std::move(info);
}

void InGameOSD::UpdateBall() {
    m_Ball.valid = false;
    PhysicsObject *ball = GetBallPhysicsObject();
    if (!ball) {
        m_PreviousVelocity.reset();
        return;
    }
    VxVector angles;
    ball->GetPosition(&m_Ball.position, &angles);
    ball->GetVelocity(&m_Ball.velocity, &m_Ball.angularVelocity);
    m_Ball.valid = true;
    m_Ball.hasDelta = m_PreviousVelocity.has_value();
    if (m_Ball.hasDelta) {
        m_Ball.deltaVelocity = m_Ball.velocity - *m_PreviousVelocity;
    }
    m_PreviousVelocity = m_Ball.velocity;
}

void InGameOSD::UpdateInputHistory() {
    m_KeyHistory.push_back(ReadGameKeys());
    while (m_KeyHistory.size() > kKeyHistoryFrames) {
        m_KeyHistory.pop_front();
    }
}

int InGameOSD::ReadGameKeys() const {
    GameInterface *game = m_Engine->GetGameInterface();
    auto *input = game->GetInputManager();
    if (!input) {
        return 0;
    }
    const CKKEYBOARD keys[] = {
        game->RemapKey(CKKEY_UP), game->RemapKey(CKKEY_DOWN), game->RemapKey(CKKEY_LEFT),
        game->RemapKey(CKKEY_RIGHT), game->RemapKey(CKKEY_LSHIFT), game->RemapKey(CKKEY_SPACE),
        CKKEY_Q, CKKEY_ESCAPE,
    };
    int mask = 0;
    for (int bit = 0; bit < 8; ++bit) {
        if (input->IsKeyDown(keys[bit])) {
            mask |= 1 << bit;
        }
    }
    return mask;
}

PhysicsObject *InGameOSD::GetBallPhysicsObject() const {
    GameInterface *game = m_Engine->GetGameInterface();
    CK3dEntity *ball = game ? game->GetActiveBall() : nullptr;
    return ball ? game->GetPhysicsObject(ball) : nullptr;
}

// --- Panels ---

void InGameOSD::DrawStatusPanel() {
    ImGui::TextColored(m_Frame.modeColor, "%s", m_Frame.mode);
    if (!m_Frame.project.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(kDim, "%s", m_Frame.project.c_str());
    }
    if (m_Frame.frame) {
        if (m_Frame.total) {
            ImGui::Text("Frame %zu / %zu", *m_Frame.frame, *m_Frame.total);
        } else {
            ImGui::Text("Frame %zu", *m_Frame.frame);
        }
    }

    GameInterface *game = m_Engine->GetGameInterface();
    const float frameTime = game->GetTimeManager() ? game->GetTimeManager()->GetLastDeltaTime() : 0.0f;
    // BML's speedrun timer, in milliseconds.
    const long long srMs = std::llround(static_cast<double>(game->GetSRScore()));
    ImGui::Text("dt %.4f ms   SR %lld:%02lld.%03lld", frameTime,
                srMs / 60000, (srMs / 1000) % 60, srMs % 1000);
    ImGui::Text("Level %d   Sector %d   Lives %d", game->GetCurrentLevel(), game->GetCurrentSector(),
                game->GetLifeCount());
}

void InGameOSD::DrawBallPanel() {
    if (!m_Ball.valid) {
        ImGui::TextColored(kDim, "Ball: none");
        return;
    }
    const VxVector &p = m_Ball.position;
    const VxVector &v = m_Ball.velocity;
    ImGui::Text("Pos   %10.4f %10.4f %10.4f", p.x, p.y, p.z);
    ImGui::Text("Vel   %10.4f %10.4f %10.4f", v.x, v.y, v.z);
    const float horizontal = std::sqrt(v.x * v.x + v.z * v.z);
    ImGui::Text("Speed %10.4f   H %.4f   V %.4f", v.Magnitude(), horizontal, v.y);
    if (m_Ball.hasDelta) {
        const VxVector &d = m_Ball.deltaVelocity;
        ImGui::Text("dVel  %10.4f %10.4f %10.4f   |dv| %.4f", d.x, d.y, d.z, d.Magnitude());
    }
    ImGui::Text("Spin  %10.4f", m_Ball.angularVelocity.Magnitude());
}

void InGameOSD::DrawInputPanel() {
    // Columns: kTimelinePast frames back, this frame, kTimelineFuture frames ahead. During
    // record playback every column is the record's input (future frames included); otherwise
    // the past columns show what the game saw and the future is unknown.
    constexpr int kColumns = kTimelinePast + 1 + kTimelineFuture;
    std::array<std::optional<int>, kColumns> columns{};
    auto *records = m_Engine->GetServiceProvider().Resolve<RecordPlayer>();
    const bool fromRecord = m_Frame.fromRecord && m_Frame.frame && records;
    for (int c = 0; c < kColumns; ++c) {
        const int offset = c - kTimelinePast;
        if (fromRecord) {
            const long long frame = static_cast<long long>(*m_Frame.frame) + offset;
            RecordFrameData data;
            if (frame >= 0 && records->GetFrameInput(static_cast<size_t>(frame), &data)) {
                columns[c] = data.keyStates;
            }
        } else if (offset <= 0) {
            const long long index = static_cast<long long>(m_KeyHistory.size()) - 1 + offset;
            if (index >= 0) {
                columns[c] = m_KeyHistory[static_cast<size_t>(index)];
            }
        }
    }

    ImDrawList *draw = ImGui::GetWindowDrawList();
    const float fontSize = ImGui::GetFontSize();
    const float cell = std::floor(fontSize * 0.85f);
    const float gap = std::max(1.0f, std::floor(fontSize * 0.12f));
    const float labelWidth = ImGui::CalcTextSize("Sp").x + fontSize * 0.4f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float gridX = origin.x + labelWidth;
    const float gridY = origin.y + fontSize + gap; // below the frame-number header

    const ImU32 emptyColor = ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.25f, 0.6f));
    const ImU32 unknownColor = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 0.35f));
    const ImU32 pastColor = ImGui::GetColorU32(ImVec4(0.2f, 0.65f, 0.2f, 0.9f));
    const ImU32 nowColor = ImGui::GetColorU32(ImVec4(0.35f, 1.0f, 0.35f, 1.0f));
    const ImU32 futureColor = ImGui::GetColorU32(ImVec4(0.3f, 0.6f, 1.0f, 0.9f));
    const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 dimText = ImGui::GetColorU32(kDim);

    // Frame numbers above this frame and the last column.
    if (m_Frame.frame) {
        auto header = [&](int c, ImU32 color) {
            const long long frame = static_cast<long long>(*m_Frame.frame) + (c - kTimelinePast);
            if (frame < 0) {
                return;
            }
            char text[24];
            std::snprintf(text, sizeof(text), "%lld", frame);
            const float x = gridX + c * (cell + gap);
            draw->AddText(ImVec2(x, origin.y), color, text);
        };
        header(kTimelinePast, textColor);
        if (fromRecord) {
            header(kColumns - 1, dimText);
        }
    }

    for (size_t r = 0; r < std::size(kKeyRows); ++r) {
        const float y = gridY + r * (cell + gap);
        draw->AddText(ImVec2(origin.x, y + (cell - fontSize) * 0.5f), dimText, kKeyRows[r].label);
        for (int c = 0; c < kColumns; ++c) {
            const ImVec2 min(gridX + c * (cell + gap), y);
            const ImVec2 max(min.x + cell, min.y + cell);
            ImU32 color = unknownColor;
            if (columns[c]) {
                const bool pressed = (*columns[c] >> kKeyRows[r].bit) & 1;
                if (!pressed) {
                    color = emptyColor;
                } else if (c < kTimelinePast) {
                    color = pastColor;
                } else if (c == kTimelinePast) {
                    color = nowColor;
                } else {
                    color = futureColor;
                }
            }
            draw->AddRectFilled(min, max, color, 2.0f);
        }
    }
    // Outline this frame's column.
    const float nowX = gridX + kTimelinePast * (cell + gap);
    const float gridHeight = std::size(kKeyRows) * (cell + gap) - gap;
    draw->AddRect(ImVec2(nowX - gap, gridY - gap), ImVec2(nowX + cell + gap, gridY + gridHeight + gap),
                  textColor, 2.0f, 0, 1.5f);

    ImGui::Dummy(ImVec2(labelWidth + kColumns * (cell + gap), fontSize + gap + gridHeight + gap));
}

void InGameOSD::DrawValidationPanel() {
    auto *validation = m_Engine->GetServiceProvider().Resolve<ValidationService>();
    if (!validation) {
        return;
    }
    const ValidationLiveStatus &status = validation->GetLiveStatus();
    if (!status.hasReference) {
        ImGui::TextColored(kDim, "Validation: recording a reference (%zu frames)", status.capturedFrames);
        return;
    }
    const char *mode = status.exact ? "exact" : "tolerance";
    if (status.diverged) {
        // Validation frames count from the level start, like the dump and its report; both
        // advance once per game frame, so the playback frame is a constant offset away.
        const size_t dumpFrame = status.firstDivergenceFrame;
        if (m_Frame.frame && status.capturedFrames > 0 && *m_Frame.frame + 1 >= status.capturedFrames) {
            const size_t offset = *m_Frame.frame + 1 - status.capturedFrames;
            ImGui::TextColored(kBad, "DESYNC at frame %zu", dumpFrame + offset);
            ImGui::SameLine();
            ImGui::TextColored(kDim, "(dump frame %zu, %s)", dumpFrame, mode);
        } else {
            ImGui::TextColored(kBad, "DESYNC at dump frame %zu", dumpFrame);
            ImGui::SameLine();
            ImGui::TextColored(kDim, "(%s)", mode);
        }
    } else if (status.pastReferenceEnd) {
        ImGui::TextColored(kNote, "MATCH, past the reference's %zu frames", status.referenceFrames);
    } else {
        ImGui::TextColored(kGood, "MATCH %zu / %zu", status.capturedFrames, status.referenceFrames);
        ImGui::SameLine();
        ImGui::TextColored(kDim, "(%s)", mode);
    }
    if (!status.pastReferenceEnd && status.capturedFrames > 0) {
        ImGui::Text("Error  pos %.6g   vel %.6g", status.positionError, status.velocityError);
    }
}

void InGameOSD::DrawWatchesPanel() {
    auto *watches = m_Engine->GetServiceProvider().Resolve<HudWatches>();
    if (!watches) {
        return;
    }
    for (const auto &entry : watches->Entries()) {
        ImGui::TextColored(kDim, "%s", entry.name.c_str());
        ImGui::SameLine();
        ImGui::TextUnformatted(entry.value.c_str());
    }
}

// --- Configuration ---

bool InGameOSD::IsPanelVisible(OSDPanel panel) const {
    const size_t index = static_cast<size_t>(panel);
    return index < m_PanelVisible.size() && m_PanelVisible[index];
}

void InGameOSD::TogglePanel(OSDPanel panel) {
    SetPanelVisible(panel, !IsPanelVisible(panel));
}

void InGameOSD::SetPanelVisible(OSDPanel panel, bool visible) {
    const size_t index = static_cast<size_t>(panel);
    if (index < m_PanelVisible.size()) {
        m_PanelVisible[index] = visible;
    }
}

void InGameOSD::SetPosition(float x, float y) {
    m_PosX = (std::max)(0.0f, (std::min)(1.0f, x));
    m_PosY = (std::max)(0.0f, (std::min)(1.0f, y));
}
