#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>

// Forward declarations
class TASEngine;
class EventManager;
class IBML;
class BallanceTAS;

/**
 * @struct RawInputState
 * @brief A snapshot of the real keyboard's state for a single frame.
 */
struct RawInputState {
    bool keyUp = false;
    bool keyDown = false;
    bool keyLeft = false;
    bool keyRight = false;
    bool keyShift = false;
    bool keySpace = false;
    bool keyQ = false;
    bool keyEsc = false;

    bool operator==(const RawInputState &other) const {
        return keyUp == other.keyUp &&
            keyDown == other.keyDown &&
            keyLeft == other.keyLeft &&
            keyRight == other.keyRight &&
            keyShift == other.keyShift &&
            keySpace == other.keySpace &&
            keyQ == other.keyQ &&
            keyEsc == other.keyEsc;
    }

    bool operator!=(const RawInputState &other) const {
        return !(*this == other);
    }

    bool HasAnyKey() const {
        return keyUp || keyDown || keyLeft || keyRight ||
            keyShift || keySpace || keyQ || keyEsc;
    }
};

/**
 * @struct GameEvent
 * @brief A record of a significant game event that occurred on a specific frame.
 */
struct GameEvent {
    std::string eventName;
    int eventData = 0; // For events like checkpoint ID

    GameEvent(const std::string &name, int data = 0)
        : eventName(name), eventData(data) {}
};

/**
 * @struct RawFrameData
 * @brief The core data unit for the recorder, storing all relevant information for one frame.
 */
struct RawFrameData {
    uint32_t frameIndex = 0;
    RawInputState inputState;
    std::vector<GameEvent> events;

    // Physics data for validation/debugging
    float ballSpeed = 0.0f;
    bool isOnGround = false;
};

/**
 * @class Recorder
 * @brief Captures player input and game events on a per-frame basis.
 *
 * During a recording session, the Recorder's Tick() method is called every frame.
 * It captures the real (un-synthesized) player input and any significant game
 * events that occurred, storing them in a sequence of RawFrameData. This sequence
 * is later used by the ScriptGenerator to create a Lua script.
 *
 * The Recorder handles its own lifecycle and auto-generation, similar to how
 * replay is handled by the TAS engine.
 */
class Recorder {
public:
    explicit Recorder(TASEngine *engine);
    ~Recorder() = default;

    // Recorder is not copyable or movable
    Recorder(const Recorder &) = delete;
    Recorder &operator=(const Recorder &) = delete;

    /**
     * @brief Starts a new recording session. Clears any previous data.
     */
    void Start();

    /**
     * @brief Stops the current recording session and returns the captured data.
     * If auto-generation is enabled, automatically generates a TAS script.
     * @return A vector of RawFrameData representing the entire recording.
     */
    std::vector<RawFrameData> Stop();

    /**
     * @brief Captures the data for the current frame. Called by TASEngine::Tick().
     */
    void Tick();

    /**
     * @brief A callback for the TASEngine to notify the recorder of a game event.
     * @param eventName The name of the event that occurred.
     * @param eventData Optional data associated with the event.
     */
    void OnGameEvent(const std::string &eventName, int eventData = 0);

    /**
     * @brief Checks if the recorder is currently recording.
     * @return True if recording is active.
     */
    bool IsRecording() const { return m_IsRecording; }

    /**
     * @brief Gets the current frame count since recording started.
     * @return Current frame index.
     */
    uint32_t GetCurrentFrame() const { return m_CurrentTick; }

    /**
     * @brief Gets the total number of frames recorded.
     * @return Total recorded frames.
     */
    size_t GetTotalFrames() const { return m_Frames.size(); }

    // --- Configuration ---

    /**
     * @brief Sets whether to automatically generate a script when recording stops.
     * @param autoGenerate True to auto-generate, false to just capture data.
     */
    void SetAutoGenerate(bool autoGenerate) { m_AutoGenerateOnStop = autoGenerate; }

    /**
     * @brief Gets the auto-generation setting.
     * @return True if auto-generation is enabled.
     */
    bool GetAutoGenerate() const { return m_AutoGenerateOnStop; }

    /**
     * @brief Sets the default author name for generated scripts.
     * @param author The default author name.
     */
    void SetDefaultAuthor(const std::string &author) { m_DefaultAuthor = author; }

    /**
     * @brief Gets the default author name.
     * @return The default author name.
     */
    const std::string &GetDefaultAuthor() const { return m_DefaultAuthor; }

    /**
     * @brief Sets a callback to be called when recording starts/stops.
     * @param callback Function to call with recording state.
     */
    void SetStatusCallback(std::function<void(bool)> callback) {
        m_StatusCallback = std::move(callback);
    }

    size_t GetMaxFrames() const { return m_MaxFrames; }

    void SetMaxFrames(size_t maxFrames) {
        m_MaxFrames = maxFrames;
        m_WarnedMaxFrames = false; // Reset warning state
    }

    float GetDeltaTime() const {
        return m_DeltaTime;
    }

    void SetUpdateRate(float tickPerSecond) {
        m_DeltaTime = 1 / tickPerSecond * 1000; // Convert to milliseconds
    }

private:
    /**
     * @brief Generates a TAS script from the current recorded frames.
     * @return True if generation was successful.
     */
    bool GenerateScript();

    /**
     * @brief Generates an automatic project name with timestamp.
     * @return A project name string with current date and time.
     */
    std::string GenerateAutoProjectName() const;

    /**
     * @brief Captures the current state of the physical keyboard.
     * This uses the original input methods to get unmodified player input.
     * @return A RawInputState struct.
     */
    RawInputState CaptureRealInput() const;

    /**
     * @brief Captures additional physics data for debugging/validation.
     * @param frameData The frame data to populate with physics info.
     */
    void CapturePhysicsData(RawFrameData &frameData) const;

    /**
     * @brief Notifies UI/callbacks about recording state changes.
     * @param isRecording New recording state.
     */
    void NotifyStatusChange(bool isRecording);

    // Core references
    TASEngine *m_Engine;
    BallanceTAS *m_Mod;
    IBML *m_BML;

    // Recording state
    bool m_IsRecording = false;
    uint32_t m_CurrentTick = 0;

    // Configuration
    bool m_AutoGenerateOnStop = true;  // Auto-generate by default
    std::string m_DefaultAuthor = "Player";
    float m_DeltaTime = 1 / 132.0f * 1000;

    // Recorded data
    std::vector<RawFrameData> m_Frames;
    std::vector<GameEvent> m_PendingEvents; // Events waiting to be assigned to a frame

    // Callbacks
    std::function<void(bool)> m_StatusCallback;

    // Performance tracking
    size_t m_MaxFrames = 100000; // Limit to prevent memory issues
    bool m_WarnedMaxFrames = false;
};
