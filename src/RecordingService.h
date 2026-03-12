#pragma once

#include <cstddef>
#include <vector>

#include "Result.h"
#include "HookManager.h"
#include "EventBus.h"

// Forward declarations
class ServiceProvider;
class Recorder;
class InputSystem;
class GameInterface;
struct FrameData;
struct StartLevelEvent;

/**
 * @struct RecordingResult
 * @brief Holds the outcome of a completed recording session.
 */
struct RecordingResult {
    std::vector<FrameData> frames;
    size_t totalFrames = 0;
};

/**
 * @class RecordingService
 * @brief Owns the full recording lifecycle — start, tick, stop, cleanup.
 *
 * Replaces: RecordingController + StandardRecorder strategy + IRecordingStrategy.
 *
 * Uses HookManager for per-frame callbacks (RAII-guarded) and directly drives
 * the Recorder subsystem from the ServiceContainer. Hook guards are released on
 * stop or destruction, so callbacks never leak.
 *
 * The state machine owns pending/active truth. This service is an execution
 * primitive: prepare, activate, stop, and clean up.
 */
class RecordingService {
public:
    explicit RecordingService(ServiceProvider *provider);
    ~RecordingService();

    RecordingService(const RecordingService &) = delete;
    RecordingService &operator=(const RecordingService &) = delete;

    /**
     * @brief Wire the service to the EventBus so it can react to level-start.
     * @param bus  The application-wide EventBus. Pointer must outlive this service.
     */
    void SetEventBus(EventBus *bus);

    /**
     * @brief Wire the service to the HookManager for per-frame tick callbacks.
     * @param hookMgr  The application-wide HookManager. Pointer must outlive this service.
     */
    void SetHookManager(HookManager *hookMgr);

    /**
     * @brief Begin a recording session.
     *
     * If a level is not yet loaded the caller should keep the state machine in
     * PendingRecord; when StartLevelEvent fires, call ActivateRecording().
     *
     * @param useValidation  If true, capture additional physics data per frame.
     * @return Ok on success, Error if preconditions aren't met.
     */
    Result<void> PrepareRecording(bool useValidation = false);

    /**
     * @brief Activate a pending recording (called when the level actually loads).
     *
     * Sets up hook callbacks and starts the Recorder. Typically invoked in
     * response to a StartLevelEvent.
     */
    Result<void> ActivateRecording();

    /**
     * @brief Stop the current recording, returning captured frame data.
     * @return RecordingResult on success, Error if not recording.
     */
    Result<RecordingResult> StopRecordingGraceful();

    /**
     * @brief Immediately stop recording without script generation (for shutdown).
     */
    void StopRecordingImmediate();

    // --- Queries ---
    bool IsRecording() const { return m_IsRecording; }
    bool IsPrepared() const { return m_IsPrepared; }
    bool IsValidation() const { return m_UseValidation; }
    size_t GetFrameCount() const;
    size_t GetCurrentTick() const { return m_CurrentTick; }

private:
    void InstallHookCallbacks();
    void RemoveHookCallbacks();
    void SetupInputSystem();
    void CleanupInputSystem();
    ServiceProvider *m_ServiceProvider;
    EventBus *m_EventBus = nullptr;
    HookManager *m_HookManager = nullptr;

    // Cached subsystem pointers (ServiceContainer owns the objects)
    Recorder *m_Recorder = nullptr;
    InputSystem *m_InputSystem = nullptr;
    GameInterface *m_GameInterface = nullptr;

    // State
    bool m_IsRecording = false;
    bool m_IsPrepared = false;
    bool m_UseValidation = false;
    size_t m_CurrentTick = 0;

    // RAII hook guards — released on stop or destruction
    ScopedCallback m_PostTickGuard;
    ScopedCallback m_PostInputGuard;

    // EventBus is kept for symmetry with other services and future lifecycle events.
};
