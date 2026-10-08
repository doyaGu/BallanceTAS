/**
 * @file ValidationService.cpp
 * @brief Implementation of ValidationService - validation recording + reproducibility comparison.
 */

#include "ValidationService.h"

#include "GameEvents.h"
#include "EventBus.h"
#include "Logger.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

ValidationService::ValidationService(Recorder &recorder, EventBus &eventBus,
                                     HookManager &hookManager)
    : m_Recorder(recorder),
      m_EventBus(eventBus),
      m_HookManager(hookManager) {
}

ValidationService::~ValidationService() {
    if (m_IsActive) {
        StopImmediate();
    }
}

Result<void> ValidationService::Start(const std::string &outputPath,
                                      const std::string &referencePath) {
    if (m_Recorder.IsRecording()) {
        return Result<void>::Error(
            "Cannot start validation while another recording is active", "state");
    }

    // Load the reference trace (a previous validation dump with physics) before
    // starting the live capture. The recorder is idle here, so loading into it
    // is safe; the frames are copied out and the recorder is cleared below.
    m_ReferenceFrames.clear();
    if (!referencePath.empty()) {
        if (!m_Recorder.LoadFrameData(referencePath, true)) {
            m_Recorder.ClearFrameData();
            return Result<void>::Error(
                "Failed to load validation reference: " + referencePath, "io");
        }
        m_ReferenceFrames = m_Recorder.GetFrames();
        if (m_ReferenceFrames.empty()) {
            m_Recorder.ClearFrameData();
            return Result<void>::Error(
                "Validation reference contains no frames: " + referencePath, "validation");
        }
    }

    m_OutputPath = outputPath;

    m_Recorder.SetAutoGenerate(false);
    m_Recorder.ClearFrameData();
    m_Recorder.Start();

    m_IsActive = true;
    m_CaptureFrame = 0;
    m_CaptureGuard = m_HookManager.RegisterPostInputCallback(
        [this](CKInputManager *inputManager) {
            if (!m_IsActive || !m_Recorder.IsRecording() || !inputManager) {
                return;
            }
            m_Recorder.Tick(m_CaptureFrame++, inputManager->GetKeyboardState());
        });
    m_LastReport = ValidationReport{};
    if (m_ReferenceFrames.empty()) {
        Log::Info("ValidationService: Started validation recording - output: %s",
                  outputPath.c_str());
    } else {
        Log::Info("ValidationService: Started validation recording - output: %s "
                  "(comparing against %s, %zu reference frames)",
                  outputPath.c_str(), referencePath.c_str(), m_ReferenceFrames.size());
    }
    m_EventBus.Publish(ValidationStartedEvent{outputPath});
    return Result<void>::Ok();
}

Result<void> ValidationService::Stop() {
    if (!m_IsActive) {
        return Result<void>::Error("Validation recording is not active", "state");
    }

    if (!m_Recorder.IsRecording()) {
        m_IsActive = false;
        return Result<void>::Error("Recorder state inconsistent", "state");
    }

    m_CaptureGuard.Reset();
    auto liveFrames = m_Recorder.Stop();

    // Write the validation dump (existing behaviour).
    const std::filesystem::path path = std::filesystem::path(m_OutputPath) /
        ("validation_" + std::to_string(std::time(nullptr)) + ".txt");

    bool ok = m_Recorder.DumpFrameData(path.string(), true);

    // Run the reproducibility comparison when a reference trace is available.
    if (!m_ReferenceFrames.empty()) {
        m_LastReport = CompareFrameData(liveFrames, m_ReferenceFrames, m_Config);
        m_LastReport.dumpPath = path.string();
        m_LastReport.reportPath = path.string() + ".report.txt";
        ok = WriteReportFile(m_LastReport.reportPath) && ok;
        LogReport();
    } else {
        m_LastReport = ValidationReport{};
        m_LastReport.liveFrames = liveFrames.size();
        m_LastReport.dumpPath = path.string();
    }

    if (ok) {
        Log::Info("ValidationService: Completed - %zu frames, dump: %s",
                  liveFrames.size(), path.string().c_str());
    } else {
        Log::Error("ValidationService: Failed to write validation output for %s",
                   path.string().c_str());
    }

    m_EventBus.Publish(ValidationStoppedEvent{path.string(), ok});

    m_IsActive = false;
    m_OutputPath.clear();
    m_ReferenceFrames.clear();
    return ok ? Result<void>::Ok()
              : Result<void>::Error("Failed to write validation dump", "io");
}

void ValidationService::StopImmediate() {
    m_CaptureGuard.Reset();
    if (m_Recorder.IsRecording()) {
        m_Recorder.Stop();
    }
    if (m_IsActive) {
        m_EventBus.Publish(ValidationStoppedEvent{m_OutputPath, false});
    }
    m_IsActive = false;
    m_CaptureFrame = 0;
    m_OutputPath.clear();
    m_ReferenceFrames.clear();
}

void ValidationService::LogReport() const {
    const auto &r = m_LastReport;
    if (r.comparedFrames == 0) {
        Log::Warn("ValidationService: no frames to compare (live=%zu, reference=%zu)",
                  r.liveFrames, r.referenceFrames);
        return;
    }
    if (r.identical) {
        Log::Info("ValidationService: MATCH - %zu/%zu frames identical "
                  "(max pos err %.4f, max vel err %.4f)",
                  r.comparedFrames, r.comparedFrames,
                  r.maxPositionError, r.maxVelocityError);
    } else {
        Log::Warn("ValidationService: DIVERGENCE - first divergent frame %zu, "
                  "%zu/%zu frames diverged (max pos err %.4f @%zu, max vel err %.4f @%zu)",
                  r.firstDivergenceFrame, r.divergentFrames, r.comparedFrames,
                  r.maxPositionError, r.maxPositionFrame,
                  r.maxVelocityError, r.maxVelocityFrame);
    }
}

bool ValidationService::WriteReportFile(const std::string &path) const {
    std::ofstream f(path);
    if (!f.is_open()) {
        Log::Error("ValidationService: Failed to write report %s", path.c_str());
        return false;
    }

    const auto &r = m_LastReport;
    f << "# Validation Comparison Report\n";
    f << "# Generated: " << std::time(nullptr) << "\n\n";
    f << "identical: " << (r.identical ? "true" : "false") << "\n";
    f << "compared_frames: " << r.comparedFrames << "\n";
    f << "reference_frames: " << r.referenceFrames << "\n";
    f << "live_frames: " << r.liveFrames << "\n";
    f << "divergent_frames: " << r.divergentFrames << "\n";
    if (r.divergentFrames > 0) {
        f << "first_divergence_frame: " << r.firstDivergenceFrame << "\n";
    }
    f << std::fixed << std::setprecision(4);
    f << "max_position_error: " << r.maxPositionError
      << "  (frame " << r.maxPositionFrame << ")\n";
    f << "max_velocity_error: " << r.maxVelocityError
      << "  (frame " << r.maxVelocityFrame << ")\n";
    f << "position_epsilon: " << m_Config.positionEpsilon << "\n";
    f << "velocity_epsilon: " << m_Config.velocityEpsilon << "\n";

    f.flush();
    f.close();
    Log::Info("ValidationService: comparison report written to %s", path.c_str());
    return true;
}
