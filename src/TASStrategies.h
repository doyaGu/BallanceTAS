#pragma once

#include "Result.h"
#include "TASProject.h"
#include <functional>
#include <memory>
#include <vector>
#include <cstdint>

// Forward declarations
class ServiceProvider;
struct FrameData;

// ============================================================================
// Playback Strategy Interface (Strategy Pattern)
// ============================================================================
class IPlaybackStrategy {
public:
    virtual ~IPlaybackStrategy() = default;

    // Playback type
    enum class Type {
        Script, // Lua script playback
        Record  // Binary record playback
    };

    // Initialize strategy
    virtual Result<void> Initialize() = 0;

    // Load and start playing project
    virtual Result<void> LoadAndPlay(TASProject *project) = 0;

    // Called every frame
    virtual void Tick() = 0;

    // Stop playback
    virtual void Stop() = 0;

    // Pause/Resume
    virtual void Pause() = 0;
    virtual void Resume() = 0;

    // Status query
    virtual bool IsPlaying() const = 0;
    virtual bool IsPaused() const = 0;
    virtual Type GetType() const = 0;

    // Playback progress
    virtual size_t GetCurrentTick() const = 0;
    virtual size_t GetTotalTicks() const = 0;
    virtual float GetProgress() const = 0; // 0.0 - 1.0

    // Observer pattern: Status change notification
    using StatusCallback = std::function<void(bool isPlaying)>;
    virtual void SetStatusCallback(StatusCallback callback) = 0;

    using ProgressCallback = std::function<void(size_t currentTick, size_t totalTicks)>;
    virtual void SetProgressCallback(ProgressCallback callback) = 0;
};

// ============================================================================
// Script Playback Strategy
// ============================================================================
class ScriptPlaybackStrategy : public IPlaybackStrategy {
public:
    explicit ScriptPlaybackStrategy(ServiceProvider *services);
    ~ScriptPlaybackStrategy() override = default;

    Result<void> Initialize() override;
    Result<void> LoadAndPlay(TASProject *project) override;
    void Tick() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;

    bool IsPlaying() const override { return m_IsPlaying && !m_IsPaused; }
    bool IsPaused() const override { return m_IsPaused; }
    Type GetType() const override { return Type::Script; }

    size_t GetCurrentTick() const override;
    size_t GetTotalTicks() const override { return 0; } // Script playback cannot predict total length
    float GetProgress() const override { return 0.0f; }

    void SetStatusCallback(StatusCallback callback) override {
        m_StatusCallback = std::move(callback);
    }

    void SetProgressCallback(ProgressCallback callback) override {
        m_ProgressCallback = std::move(callback);
    }

private:
    ServiceProvider *m_Services;
    TASProject *m_CurrentProject = nullptr;
    bool m_IsPlaying = false;
    bool m_IsPaused = false;
    size_t m_CurrentTick = 0;
    StatusCallback m_StatusCallback;
    ProgressCallback m_ProgressCallback;

    void NotifyStatusChanged();
};

// ============================================================================
// Record Playback Strategy
// ============================================================================
class RecordPlaybackStrategy : public IPlaybackStrategy {
public:
    explicit RecordPlaybackStrategy(ServiceProvider *services);
    ~RecordPlaybackStrategy() override = default;

    Result<void> Initialize() override;
    Result<void> LoadAndPlay(TASProject *project) override;
    void Tick() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;

    bool IsPlaying() const override { return m_IsPlaying && !m_IsPaused; }
    bool IsPaused() const override { return m_IsPaused; }
    Type GetType() const override { return Type::Record; }

    size_t GetCurrentTick() const override { return m_CurrentFrameIndex; }
    size_t GetTotalTicks() const override { return m_TotalFrames; }

    float GetProgress() const override {
        return m_TotalFrames > 0 ? static_cast<float>(m_CurrentFrameIndex) / m_TotalFrames : 0.0f;
    }

    void SetStatusCallback(StatusCallback callback) override {
        m_StatusCallback = std::move(callback);
    }

    void SetProgressCallback(ProgressCallback callback) override {
        m_ProgressCallback = std::move(callback);
    }

private:
    ServiceProvider *m_Services;
    std::vector<FrameData> m_Frames;
    size_t m_CurrentFrameIndex = 0;
    size_t m_TotalFrames = 0;
    bool m_IsPlaying = false;
    bool m_IsPaused = false;
    StatusCallback m_StatusCallback;
    ProgressCallback m_ProgressCallback;

    void NotifyStatusChanged();
    void NotifyProgress();
};

// ============================================================================
// Recording Strategy Interface
// ============================================================================
class IRecordingStrategy {
public:
    virtual ~IRecordingStrategy() = default;

    // Recording type
    enum class Type {
        Standard,  // Standard recording
        Validation // Validation recording (records more information)
    };

    // Start recording
    virtual Result<void> Start() = 0;

    // Record input every frame
    virtual void Tick(size_t currentTick, const unsigned char *keyboardState) = 0;

    // Stop recording and return data
    virtual Result<std::vector<FrameData>> Stop() = 0;

    // Status query
    virtual bool IsRecording() const = 0;
    virtual size_t GetFrameCount() const = 0;
    virtual Type GetType() const = 0;

    // Recording options
    struct Options {
        bool captureMouseInput = false;   // Whether to capture mouse input
        bool captureGamepadInput = false; // Whether to capture gamepad input
        bool captureTimestamps = true;    // Whether to record timestamps
        bool captureGameState = false;    // Whether to record game state (for validation)
        size_t maxFrames = 0;             // Maximum frame limit (0 = unlimited)
    };

    virtual void SetOptions(const Options &options) = 0;
    virtual const Options &GetOptions() const = 0;
};

// ============================================================================
// Standard Recording Strategy
// ============================================================================
class StandardRecorder : public IRecordingStrategy {
public:
    explicit StandardRecorder(ServiceProvider *services);
    ~StandardRecorder() override = default;

    Result<void> Start() override;
    void Tick(size_t currentTick, const unsigned char *keyboardState) override;
    Result<std::vector<FrameData>> Stop() override;

    bool IsRecording() const override { return m_IsRecording; }
    size_t GetFrameCount() const override { return m_Frames.size(); }
    Type GetType() const override { return Type::Standard; }

    void SetOptions(const Options &options) override { m_Options = options; }
    const Options &GetOptions() const override { return m_Options; }

private:
    ServiceProvider *m_Services;
    std::vector<FrameData> m_Frames;
    bool m_IsRecording = false;
    Options m_Options;

    // Previous frame keyboard state (for detecting changes)
    std::vector<unsigned char> m_PreviousKeyState;

    bool HasKeyStateChanged(const unsigned char *currentState) const;
};

// ============================================================================
// Validation Recording Strategy (Decorator Pattern)
// ============================================================================
class ValidationRecorder : public IRecordingStrategy {
public:
    explicit ValidationRecorder(std::unique_ptr<IRecordingStrategy> innerRecorder);
    ~ValidationRecorder() override = default;

    Result<void> Start() override;
    void Tick(size_t currentTick, const unsigned char *keyboardState) override;
    Result<std::vector<FrameData>> Stop() override;

    bool IsRecording() const override { return m_InnerRecorder->IsRecording(); }
    size_t GetFrameCount() const override { return m_InnerRecorder->GetFrameCount(); }
    Type GetType() const override { return Type::Validation; }

    void SetOptions(const Options &options) override;
    const Options &GetOptions() const override { return m_InnerRecorder->GetOptions(); }

private:
    std::unique_ptr<IRecordingStrategy> m_InnerRecorder;

    // Additional validation data
    struct ValidationData {
        float ballPosition[3];
        float ballVelocity[3];
        int currentLevel;
        uint64_t timestamp;
    };

    std::vector<ValidationData> m_ValidationData;

    void CaptureValidationData(size_t currentTick);
};
