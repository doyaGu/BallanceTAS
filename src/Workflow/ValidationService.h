#pragma once

#include "Result.h"
#include "Recorder.h"
#include "HookManager.h"

#include <string>
#include <vector>

// Forward declarations
class EventBus;

/**
 * @struct ValidationConfig
 * @brief Tolerances and options for the physics comparison.
 */
struct ValidationConfig {
    /// Max allowable ball position deviation (game units) before a frame counts as diverged.
    float positionEpsilon = 1.0f;
    /// Max allowable ball velocity deviation (units/sec) before a frame counts as diverged.
    float velocityEpsilon = 2.0f;
};

/**
 * @struct ValidationReport
 * @brief Result of comparing a live validation run against a reference trace.
 *
 * Closes the record→replay verification loop: compares the ball physics captured
 * during a playback against a previously recorded reference trace, reporting the
 * first divergent frame, the divergent-frame count, and the worst-case deviation.
 */
struct ValidationReport {
    bool hasReference = false;        ///< True if a reference trace was loaded for comparison.
    bool identical = false;           ///< True when all compared frames matched within tolerance.
    size_t comparedFrames = 0;        ///< Frames actually compared (min of live/reference counts).
    size_t referenceFrames = 0;       ///< Frame count of the reference trace.
    size_t liveFrames = 0;            ///< Frame count of the live validation recording.
    size_t firstDivergenceFrame = 0;  ///< First frame exceeding tolerance (valid only when divergentFrames > 0).
    size_t divergentFrames = 0;       ///< Total frames exceeding tolerance.
    float maxPositionError = 0.0f;    ///< Largest ball position deviation observed (units).
    size_t maxPositionFrame = 0;      ///< Frame at which maxPositionError occurred.
    float maxVelocityError = 0.0f;    ///< Largest ball velocity deviation observed (units/sec).
    size_t maxVelocityFrame = 0;      ///< Frame at which maxVelocityError occurred.
    std::string dumpPath;             ///< Validation trace written by Stop().
    std::string reportPath;           ///< Comparison report path, empty when no reference was used.
};

/**
 * @class ValidationService
 * @brief Captures a validation recording during playback and verifies reproducibility.
 *
 * This service owns a post-input hook for the validation session and captures
 * live ball physics exactly once per game frame. When given a previous dump, it
 * compares the live capture frame-by-frame and
 * reports divergence — closing the TAS verification loop.
 */
class ValidationService {
public:
    ValidationService(Recorder &recorder, EventBus &eventBus, HookManager &hookManager);
    ~ValidationService();

    ValidationService(const ValidationService &) = delete;
    ValidationService &operator=(const ValidationService &) = delete;

    /**
     * @brief Start a validation recording session.
     * @param outputPath     Base directory for validation dump/report files.
     * @param referencePath  Optional path to a prior validation dump (.txt with physics)
     *                       to compare the live run against. Empty disables comparison.
     * @return Error if Recorder is already active or the reference cannot be loaded.
     *         The caller is responsible for requiring an active playback source.
     */
    Result<void> Start(const std::string &outputPath,
                       const std::string &referencePath = "");

    /**
     * @brief Stop validation recording, write the dump, and (if a reference was loaded)
     *        produce the comparison report.
     * @return Error if validation wasn't active or the dump failed.
     */
    Result<void> Stop();

    /** Immediately stop without dump or comparison (for shutdown paths). */
    void StopImmediate();

    // --- Queries ---
    bool IsActive() const { return m_IsActive; }
    const std::string &GetOutputPath() const { return m_OutputPath; }

    /** Result of the most recent Stop() comparison (empty/no-reference if none). */
    const ValidationReport &GetLastReport() const { return m_LastReport; }

    // --- Configuration ---
    void SetConfig(const ValidationConfig &config) { m_Config = config; }
    const ValidationConfig &GetConfig() const { return m_Config; }

    /** Pure frame comparison seam used by the service and regression tests. */
    static ValidationReport CompareFrameData(const std::vector<FrameData> &live,
                                             const std::vector<FrameData> &reference,
                                             const ValidationConfig &config = {});

private:
    Recorder &m_Recorder;
    EventBus &m_EventBus;
    HookManager &m_HookManager;
    ScopedCallback m_CaptureGuard;
    size_t m_CaptureFrame = 0;

    bool m_IsActive = false;
    std::string m_OutputPath;

    ValidationConfig m_Config;
    std::vector<FrameData> m_ReferenceFrames;  ///< Loaded reference trace (empty if none).
    ValidationReport m_LastReport;

    void LogReport() const;
    bool WriteReportFile(const std::string &path) const;
};
