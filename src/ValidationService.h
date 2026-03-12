#pragma once

#include "Result.h"

#include <string>

// Forward declarations
class ServiceProvider;
class Recorder;
class PlaybackService;
class EventBus;

/**
 * @class ValidationService
 * @brief Manages validation recording that runs in parallel with script playback.
 *
 * Replaces: m_ValidationEnabled / m_ValidationRecording / m_ValidationOutputPath
 *           scattered across TASEngine.
 *
 * During script playback the PlaybackService already piggybacks a Recorder::Tick
 * call when the Recorder is active. This service just owns the start / stop /
 * dump lifecycle of that secondary recording.
 */
class ValidationService {
public:
    explicit ValidationService(ServiceProvider *provider);
    ~ValidationService();

    ValidationService(const ValidationService &) = delete;
    ValidationService &operator=(const ValidationService &) = delete;

    void SetEventBus(EventBus *bus) { m_EventBus = bus; }

    /**
     * @brief Start a validation recording session.
     * @param outputPath  Base directory for validation dump files.
     * @param playback    The currently-active PlaybackService (must be playing script).
     * @return Error if playback isn't active or Recorder is already recording.
     */
    Result<void> Start(const std::string &outputPath, const PlaybackService &playback);

    /**
     * @brief Stop validation recording and write dump files.
     * @return Error if validation wasn't active.
     */
    Result<void> Stop();

    /** Immediately stop without dump (for shutdown paths). */
    void StopImmediate();

    // --- Queries ---
    bool IsActive() const { return m_IsActive; }
    const std::string &GetOutputPath() const { return m_OutputPath; }

private:
    ServiceProvider *m_ServiceProvider;
    Recorder *m_Recorder = nullptr;
    EventBus *m_EventBus = nullptr;

    bool m_IsActive = false;
    std::string m_OutputPath;
};
