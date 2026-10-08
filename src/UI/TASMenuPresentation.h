#pragma once

#include <cstddef>
#include <string>

class TASProject;

enum class TASMenuTone {
    Normal,
    Muted,
    Good,
    Warning,
    Error,
    Active,
};

struct TASMenuActionPresentation {
    std::string label;
    bool enabled = false;
    std::string disabledReason;
};

struct TASMenuRuntimeSnapshot {
    bool pendingPlayback = false;
    bool runningPlayback = false;
    bool pendingRecording = false;
    bool recording = false;
    bool pendingTranslation = false;
    bool translating = false;
    bool playbackCompleted = false;
    std::string activeProjectName;
    std::string activeProjectKey;
    bool activeProjectIsRecord = false;
    std::string activeTargetLevel;
    std::string activityDetail;
    std::string lastError;
    std::string lastInfo;
    size_t currentTick = 0;
    size_t frameCount = 0;
    float progress = 0.0f;
};

struct TASMenuStatePresentation {
    std::string label = "Idle";
    std::string detail = "Ready";
    std::string activeProjectName;
    std::string activeProjectKey;
    std::string failureReason;
    TASMenuTone tone = TASMenuTone::Normal;
    TASMenuActionPresentation stopAction;
    TASMenuActionPresentation refreshAction{"Refresh", true, ""};
    bool hasBlockingActivity = false;
};

struct TASProjectPresentation {
    std::string fullName;
    std::string displayName;
    std::string typeLabel;
    std::string scopeLabel;
    std::string triggerLabel;
    std::string levelLabel;
    std::string entryLabel;
    std::string updateRateLabel;
    std::string recordInfoLabel;
    std::string translationLabel;
    std::string validityLabel;
    std::string rowStatusLabel;
    std::string rowBadgeLabel;
    std::string typeMarkerLabel;
    std::string nextStepLabel;
    TASMenuTone rowTone = TASMenuTone::Normal;
    TASMenuActionPresentation playAction;
    TASMenuActionPresentation translateAction;
};

std::string TruncateMenuLabel(const std::string &label, size_t maxChars = 24);
/// Maps internal level keys ("Level_02") to the name shown in Ballance's level list ("Level 2").
std::string FormatLevelLabel(const std::string &levelKey);
/// Tells the user how to begin a pending TAS, e.g. "select Level 2 to start".
std::string BuildLevelStartInstruction(const std::string &targetLevel, bool isRecord);
/// In-game message telling the player how to start a pending TAS; empty when nothing is pending.
std::string BuildPendingMessage(const TASMenuRuntimeSnapshot &snapshot, const std::string &stopKeyLabel);
TASMenuStatePresentation BuildMenuStatePresentation(const TASMenuRuntimeSnapshot &snapshot);
TASProjectPresentation BuildProjectPresentation(const TASProject &project,
                                                const TASMenuStatePresentation &menuState);
