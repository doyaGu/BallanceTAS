#include "TASMenuPresentation.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <sstream>

#include "TASProject.h"

static bool IsActiveOrPending(const TASMenuStatePresentation &state) {
    return state.hasBlockingActivity;
}

static bool IsActiveProject(const TASProject &project, const TASMenuStatePresentation &state) {
    if (!state.activeProjectKey.empty()) {
        return state.activeProjectKey == project.GetPath();
    }
    return !state.activeProjectName.empty() && state.activeProjectName == project.GetName();
}

static std::string FormatUpdateRate(float updateRate) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%.0f Hz", updateRate);
    return buffer;
}

static std::string FormatRecordPlaybackDetail(const TASMenuRuntimeSnapshot &snapshot) {
    std::ostringstream stream;
    if (!snapshot.activeProjectName.empty()) {
        stream << snapshot.activeProjectName << " ";
    }

    const size_t total = snapshot.frameCount;
    const size_t current = total > 0 ? (std::min)(snapshot.currentTick, total) : snapshot.currentTick;
    stream << current;
    if (total > 0) {
        const double percent = static_cast<double>(current) * 100.0 / static_cast<double>(total);
        stream << "/" << total << " (" << static_cast<int>(std::round(percent)) << "%)";
    }

    return stream.str();
}

static std::string FormatScriptPlaybackDetail(const TASMenuRuntimeSnapshot &snapshot) {
    std::ostringstream stream;
    if (!snapshot.activeProjectName.empty()) {
        stream << snapshot.activeProjectName << " ";
    }
    stream << "tick " << snapshot.currentTick;
    return stream.str();
}

static std::string BuildRecordInfo(const TASProject &project) {
    std::ostringstream stream;
    stream << project.GetRecordFrameCount() << " frames";
    if (project.GetRecordFrameCount() == 0) {
        stream << " (empty)";
    }
    return stream.str();
}

static std::string BuildInvalidReason(const TASProject &project) {
    if (!project.GetValidationMessage().empty()) {
        return project.GetValidationMessage();
    }
    return "Invalid project";
}

static std::string BuildPlayDisabledReason(const TASProject &project, const TASMenuStatePresentation &menuState) {
    if (IsActiveOrPending(menuState)) {
        return "Stop current TAS first";
    }

    if (!project.IsValid()) {
        return BuildInvalidReason(project);
    }

    if (project.IsRecordProject() && project.GetRecordFrameCount() == 0) {
        return "Record has no frames";
    }

    if (project.IsScriptProject() && project.GetEntryScript().empty()) {
        return "Project entry script is missing";
    }

    return {};
}

static std::string BuildTranslateDisabledReason(const TASProject &project, const TASMenuStatePresentation &menuState) {
    if (IsActiveOrPending(menuState)) {
        return "Stop current TAS first";
    }

    if (project.IsRecordProject()) {
        if (!project.IsValid() || project.GetRecordFrameCount() == 0 || !project.CanBeTranslated()) {
            return project.GetTranslationCompatibilityMessage();
        }
        return {};
    }

    if (!project.CanTranslateToRecord()) {
        return project.GetScriptToRecordCompatibilityMessage();
    }

    return {};
}

static std::string BuildScopeLabel(const TASProject &project) {
    if (project.IsGlobalProject()) {
        return "GLOBAL";
    }

    if (!project.GetTargetLevel().empty()) {
        return "LEVEL:" + project.GetTargetLevel();
    }

    return "LEVEL";
}

static std::string BuildRowStatus(const TASProject &project, const TASMenuStatePresentation &state) {
    if (!project.IsValid()) {
        return "ERROR";
    }

    if (!state.hasBlockingActivity || state.activeProjectName.empty()) {
        if (project.IsGlobalProject()) {
            return "GLOBAL";
        }
        return "LEVEL";
    }

    if (IsActiveProject(project, state)) {
        if (state.label.rfind("Pending", 0) == 0) {
            return "WAIT";
        }
        if (state.label == "Recording") {
            return "REC";
        }
        if (state.label == "Translating") {
            return "TRANS";
        }
        return "RUN";
    }

    if (project.IsGlobalProject()) {
        return "GLOBAL";
    }
    return "LEVEL";
}

static std::string BuildRowBadge(const std::string &status) {
    if (status == "GLOBAL") {
        return "G";
    }
    if (status == "RECORD") {
        return "R";
    }
    if (status == "LEVEL") {
        return "L";
    }
    if (status == "ERROR") {
        return "!";
    }
    if (status == "WAIT") {
        return "...";
    }
    if (status == "TRANS") {
        return "T";
    }
    if (status == "REC") {
        return "REC";
    }
    if (status == "RUN") {
        return "RUN";
    }
    return "";
}

static TASMenuTone BuildRowTone(const TASProject &project, const TASMenuStatePresentation &state) {
    if (!project.IsValid()) {
        return TASMenuTone::Error;
    }

    if (IsActiveProject(project, state)) {
        return state.label.rfind("Pending", 0) == 0 ? TASMenuTone::Warning : TASMenuTone::Active;
    }

    if (project.IsGlobalProject()) {
        return TASMenuTone::Good;
    }

    return TASMenuTone::Normal;
}

std::string TruncateMenuLabel(const std::string &label, size_t maxChars) {
    if (label.size() <= maxChars || maxChars < 4) {
        return label;
    }

    return label.substr(0, maxChars - 3) + "...";
}

TASMenuStatePresentation BuildMenuStatePresentation(const TASMenuRuntimeSnapshot &snapshot) {
    TASMenuStatePresentation state;
    state.refreshAction = {"Refresh", true, ""};
    state.stopAction = {"Stop TAS", false, "Nothing is running"};
    state.activeProjectName = snapshot.activeProjectName;
    state.activeProjectKey = snapshot.activeProjectKey;
    state.failureReason = snapshot.lastError;

    const auto detailOrProject = [&]() {
        if (!snapshot.activityDetail.empty()) {
            return snapshot.activityDetail;
        }
        if (!snapshot.activeProjectName.empty()) {
            return snapshot.activeProjectName;
        }
        return std::string{};
    };

    if (!snapshot.lastError.empty()) {
        state.label = "Error";
        state.detail = snapshot.lastError;
        state.tone = TASMenuTone::Error;
    }

    if (snapshot.pendingPlayback) {
        state.label = "Pending Playback";
        if (!snapshot.activityDetail.empty()) {
            state.detail = snapshot.activityDetail;
        } else if (snapshot.activeProjectIsRecord) {
            state.detail = snapshot.activeProjectName.empty()
                ? "Waiting for level load"
                : "Waiting for level load: " + snapshot.activeProjectName;
        } else {
            state.detail = snapshot.activeProjectName.empty()
                ? "Waiting for script context"
                : "Waiting for script context: " + snapshot.activeProjectName;
        }
        state.tone = TASMenuTone::Warning;
        state.stopAction = {"Stop Playback", true, ""};
        state.hasBlockingActivity = true;
    } else if (snapshot.runningPlayback) {
        state.label = "Running Playback";
        if (!snapshot.activityDetail.empty()) {
            state.detail = snapshot.activityDetail;
        } else {
            state.detail = snapshot.activeProjectIsRecord
                ? FormatRecordPlaybackDetail(snapshot)
                : FormatScriptPlaybackDetail(snapshot);
        }
        state.tone = TASMenuTone::Active;
        state.stopAction = {"Stop Playback", true, ""};
        state.hasBlockingActivity = true;
    } else if (snapshot.pendingRecording) {
        state.label = "Pending Recording";
        state.detail = detailOrProject().empty() ? "Waiting for target level" : detailOrProject();
        state.tone = TASMenuTone::Warning;
        state.stopAction = {"Stop Recording", true, ""};
        state.hasBlockingActivity = true;
    } else if (snapshot.recording) {
        state.label = "Recording";
        state.detail = "Frames: " + std::to_string(snapshot.frameCount);
        state.tone = TASMenuTone::Active;
        state.stopAction = {"Stop Recording", true, ""};
        state.hasBlockingActivity = true;
    } else if (snapshot.pendingTranslation) {
        state.label = "Pending Translation";
        state.detail = detailOrProject().empty() ? "Preparing translation" : detailOrProject();
        state.tone = TASMenuTone::Warning;
        state.stopAction = {"Stop Translation", true, ""};
        state.hasBlockingActivity = true;
    } else if (snapshot.translating) {
        state.label = "Translating";
        state.detail = detailOrProject().empty() ? "Translation running" : detailOrProject();
        state.tone = TASMenuTone::Active;
        state.stopAction = {"Stop Translation", true, ""};
        state.hasBlockingActivity = true;
    } else if (snapshot.playbackCompleted) {
        state.label = "Idle";
        state.detail = snapshot.activeProjectName.empty()
            ? "Playback completed"
            : "Playback completed: " + snapshot.activeProjectName;
        state.tone = TASMenuTone::Good;
    } else if (!snapshot.lastInfo.empty()) {
        state.label = "Idle";
        state.detail = snapshot.lastInfo;
        state.tone = TASMenuTone::Good;
    } else if (snapshot.lastError.empty()) {
        state.label = "Idle";
        state.detail = "Ready";
        state.tone = TASMenuTone::Normal;
    }

    return state;
}

TASProjectPresentation BuildProjectPresentation(const TASProject &project,
                                                const TASMenuStatePresentation &menuState) {
    TASProjectPresentation presentation;
    presentation.fullName = project.GetName();
    presentation.displayName = TruncateMenuLabel(project.GetName());
    presentation.typeLabel = project.IsRecordProject() ? "RECORD" : "SCRIPT";
    presentation.typeMarkerLabel.clear();
    presentation.scopeLabel = BuildScopeLabel(project);
    presentation.triggerLabel = project.GetExecutionTrigger();
    presentation.levelLabel = project.GetTargetLevel().empty() ? "Any" : project.GetTargetLevel();
    presentation.entryLabel = project.GetEntryScript().empty() ? "(missing)" : project.GetEntryScript();
    presentation.updateRateLabel = FormatUpdateRate(project.GetUpdateRate());
    presentation.validityLabel = project.IsValid() ? "Ready" : BuildInvalidReason(project);
    presentation.rowStatusLabel = BuildRowStatus(project, menuState);
    presentation.rowBadgeLabel = BuildRowBadge(presentation.rowStatusLabel);
    presentation.rowTone = BuildRowTone(project, menuState);

    if (project.IsRecordProject()) {
        presentation.recordInfoLabel = BuildRecordInfo(project);
        presentation.translationLabel = project.GetTranslationCompatibilityMessage();
    } else {
        presentation.recordInfoLabel.clear();
        presentation.translationLabel = project.GetScriptToRecordCompatibilityMessage();
    }

    presentation.playAction.label = "Play TAS";
    presentation.playAction.disabledReason = BuildPlayDisabledReason(project, menuState);
    presentation.playAction.enabled = presentation.playAction.disabledReason.empty();

    presentation.translateAction.label = project.IsRecordProject() ? "Translate to Script" : "Translate to Record";
    presentation.translateAction.disabledReason = BuildTranslateDisabledReason(project, menuState);
    presentation.translateAction.enabled = presentation.translateAction.disabledReason.empty();

    return presentation;
}
