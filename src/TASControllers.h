/**
 * @file TASControllers.h
 * @brief Controller classes for TASEngine subsystems
 *
 * This file implements the Controller pattern to separate business logic
 * from TASEngine. Controllers coordinate subsystems and strategies to
 * implement high-level TAS operations.
 *
 * Architecture:
 * - RecordingController: Manages recording lifecycle
 * - PlaybackController: Manages playback lifecycle (Script/Record)
 * - TranslationController: Manages record-to-script translation
 *
 * Dependency Injection:
 * Controllers now have access to ServiceProvider through TASEngine::GetServiceProvider().
 * Future enhancements can resolve dependencies via the service container instead of
 * direct TASEngine accessors, enabling full decoupling.
 *
 * Example:
 *   auto provider = m_Engine->GetServiceProvider();
 *   auto recorder = provider->Resolve<Recorder>();
 *   auto inputSystem = provider->Resolve<InputSystem>();
 */

#pragma once

#include <memory>

#include "Result.h"
#include "TASProject.h"
#include "TASStrategies.h"
#include "ScriptGenerator.h"

// Forward declarations
class TASEngine;
class ServiceProvider;
class Recorder;
class RecordPlayer;
class ScriptContextManager;
class InputSystem;
class GameInterface;

// Enum forward declarations
enum class PlaybackType;

// ============================================================================
// RecordingController
// ============================================================================
/**
 * @class RecordingController
 * @brief Controls the recording subsystem lifecycle
 *
 * Responsibilities:
 * - Start/stop recording sessions
 * - Manage recording strategies (Standard/Validation)
 * - Handle recording state transitions
 * - Coordinate with InputSystem during recording
 */
class RecordingController {
public:
    /**
     * @brief Constructs RecordingController with dependency injection
     * @param provider ServiceProvider for resolving dependencies
     */
    explicit RecordingController(ServiceProvider *provider);
    ~RecordingController() = default;

    // RecordingController is not copyable or movable
    RecordingController(const RecordingController &) = delete;
    RecordingController &operator=(const RecordingController &) = delete;

    /**
     * @brief Initializes the recording controller
     * @return Result indicating success or failure
     */
    Result<void> Initialize();

    /**
     * @brief Starts a new recording session
     * @param useValidation Whether to use validation recording
     * @return Result indicating success or failure
     */
    Result<void> StartRecording(bool useValidation = false);

    /**
     * @brief Stops the current recording session
     * @param immediate If true, stops immediately without cleanup
     * @return Result containing the recorded frame data
     */
    Result<std::vector<FrameData>> StopRecording(bool immediate = false);

    /**
     * @brief Checks if currently recording
     */
    bool IsRecording() const;

    /**
     * @brief Gets the current frame count
     */
    size_t GetFrameCount() const;

    /**
     * @brief Sets recording options
     */
    void SetRecordingOptions(const IRecordingStrategy::Options &options);

private:
    ServiceProvider *m_ServiceProvider;
    std::unique_ptr<IRecordingStrategy> m_Strategy;
    bool m_IsInitialized = false;

    // Helper methods
    void SetupInputSystemForRecording();
    void CleanupAfterRecording();
};

// ============================================================================
// PlaybackController
// ============================================================================
/**
 * @class PlaybackController
 * @brief Controls the playback subsystem lifecycle
 *
 * Responsibilities:
 * - Start/stop playback (Script or Record)
 * - Manage playback strategies
 * - Handle pause/resume/seek operations
 * - Coordinate with InputSystem during playback
 */
class PlaybackController {
public:
    explicit PlaybackController(ServiceProvider *provider);
    ~PlaybackController() = default;

    // PlaybackController is not copyable or movable
    PlaybackController(const PlaybackController &) = delete;
    PlaybackController &operator=(const PlaybackController &) = delete;

    /**
     * @brief Initializes the playback controller
     */
    Result<void> Initialize();

    /**
     * @brief Starts playback of a TAS project
     * @param project The project to play
     * @param type Playback type (Script or Record)
     * @return Result indicating success or failure
     */
    Result<void> StartPlayback(TASProject *project, PlaybackType type);

    /**
     * @brief Stops the current playback
     * @param clearProject Whether to clear the loaded project
     */
    void StopPlayback(bool clearProject = true);

    /**
     * @brief Pauses the current playback
     */
    void Pause();

    /**
     * @brief Resumes paused playback
     */
    void Resume();

    /**
     * @brief Checks if currently playing
     */
    bool IsPlaying() const;

    /**
     * @brief Checks if playback is paused
     */
    bool IsPaused() const;

    /**
     * @brief Gets the current playback type
     */
    PlaybackType GetPlaybackType() const { return m_CurrentType; }

    /**
     * @brief Gets playback progress (0.0 - 1.0)
     */
    float GetProgress() const;

private:
    ServiceProvider *m_ServiceProvider;
    std::unique_ptr<IPlaybackStrategy> m_Strategy;
    TASProject *m_CurrentProject = nullptr;
    PlaybackType m_CurrentType; // Initialized in constructor
    bool m_IsInitialized = false;

    // Helper methods
    void SetupInputSystemForPlayback(PlaybackType type);
    void CleanupAfterPlayback();
    Result<std::unique_ptr<IPlaybackStrategy>> CreateStrategy(PlaybackType type);
};

// ============================================================================
// TranslationController
// ============================================================================
/**
 * @class TranslationController
 * @brief Controls the translation process (Record → Script)
 *
 * Responsibilities:
 * - Coordinate record playback for translation
 * - Generate Lua scripts from recorded input
 * - Manage translation state
 */
class TranslationController {
public:
    explicit TranslationController(ServiceProvider *provider);
    ~TranslationController() = default;

    // TranslationController is not copyable or movable
    TranslationController(const TranslationController &) = delete;
    TranslationController &operator=(const TranslationController &) = delete;

    /**
     * @brief Initializes the translation controller
     */
    Result<void> Initialize();

    /**
     * @brief Starts translation of a record project to script
     * @param project The record project to translate
     * @param options Script generation options
     * @return Result indicating success or failure
     */
    Result<void> StartTranslation(TASProject *project, const GenerationOptions &options);

    /**
     * @brief Stops the current translation
     * @param clearProject Whether to clear the loaded project
     */
    void StopTranslation(bool clearProject = true);

    /**
     * @brief Checks if currently translating
     */
    bool IsTranslating() const;

    /**
     * @brief Gets translation progress (0.0 - 1.0)
     */
    float GetProgress() const;

private:
    ServiceProvider *m_ServiceProvider;
    TASProject *m_CurrentProject = nullptr;
    GenerationOptions m_GenerationOptions;
    bool m_IsTranslating = false;
    bool m_IsInitialized = false;

    // Helper methods
    void OnTranslationPlaybackComplete();
};
