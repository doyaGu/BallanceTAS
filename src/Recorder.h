#pragma once

#include <cstdint>
#include <utility>
#include <vector>
#include <string>
#include <memory>
#include <functional>

#include <CKInputManager.h>

// Forward declarations
class TASEngine;
class EventManager;
class IBML;
class BallanceTAS;
struct GenerationOptions;

/**
 * @struct RawInputState
 * @brief A snapshot of the real keyboard's state for a single frame.
 * Each field contains the raw keyboard state value (KS_IDLE, KS_PRESSED, KS_RELEASED).
 */
struct RawInputState {
    uint8_t keyUp = KS_IDLE;
    uint8_t keyDown = KS_IDLE;
    uint8_t keyLeft = KS_IDLE;
    uint8_t keyRight = KS_IDLE;
    uint8_t keyShift = KS_IDLE;
    uint8_t keySpace = KS_IDLE;
    uint8_t keyQ = KS_IDLE;
    uint8_t keyEsc = KS_IDLE;

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

    // Check if any key has the PRESSED bit set
    bool HasAnyPressed() const {
        return (keyUp & KS_PRESSED) ||
               (keyDown & KS_PRESSED) ||
               (keyLeft & KS_PRESSED) ||
               (keyRight & KS_PRESSED) ||
               (keyShift & KS_PRESSED) ||
               (keySpace & KS_PRESSED) ||
               (keyQ & KS_PRESSED) ||
               (keyEsc & KS_PRESSED);
    }

    // Check if any key has the RELEASED bit set
    bool HasAnyReleased() const {
        return (keyUp & KS_RELEASED) ||
               (keyDown & KS_RELEASED) ||
               (keyLeft & KS_RELEASED) ||
               (keyRight & KS_RELEASED) ||
               (keyShift & KS_RELEASED) ||
               (keySpace & KS_RELEASED) ||
               (keyQ & KS_RELEASED) ||
               (keyEsc & KS_RELEASED);
    }
};

/**
 * @struct GameEvent
 * @brief A record of a significant game event that occurred on a specific frame.
 */
struct GameEvent {
    size_t frame = 0;
    std::string eventName;
    int eventData = 0; // For events like checkpoint ID

    explicit GameEvent(size_t frameNum, std::string name, int data = 0)
        : frame(frameNum), eventName(std::move(name)), eventData(data) {}
};

/**
 * @struct PhysicsData
 * @brief Comprehensive physics data for better analysis and partitioning.
 */
struct PhysicsData {
    // Position and movement
    VxVector position = VxVector(0, 0, 0);
    VxVector velocity = VxVector(0, 0, 0);
    VxVector angularVelocity = VxVector(0, 0, 0);

    // Derived values
    float speed = 0.0f;
    float angularSpeed = 0.0f;
};

/**
 * @struct FrameData
 * @brief The core data unit for the recorder, storing all relevant information for one frame.
 */
struct FrameData {
    size_t frameIndex = 0;
    RawInputState inputState;
    std::vector<GameEvent> events;

    // Physics data
    PhysicsData physics;

    // Frame timing
    float deltaTime = 0.0f;
};

/**
 * @class Recorder
 * @brief Captures player input and game events on a per-frame basis.
 *
 * During a recording session, the Recorder's Tick() method is called every frame.
 * It captures the real (unsynthesized) player input and any significant game
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
    std::vector<FrameData> Stop();

    /**
     * @brief Captures the data for the current frame. Called by TASEngine::Tick().
     * @param currentTick The current game tick/frame index.
     * @param keyboardState The current keyboard state array.
     */
    void Tick(size_t currentTick, const unsigned char *keyboardState);

    /**
     * @brief A callback for the TASEngine to notify the recorder of a game event.
     * @param currentTick The current game tick/frame index.
     * @param eventName The name of the event that occurred.
     * @param eventData Optional data associated with the event.
     */
    void OnGameEvent(size_t currentTick, const std::string &eventName, int eventData = 0);

    /**
     * @brief Checks if the recorder is currently recording.
     * @return True if recording is active.
     */
    bool IsRecording() const { return m_IsRecording; }

    /**
     * @brief Gets the total number of frames recorded.
     * @return Total recorded frames.
     */
    size_t GetTotalFrames() const { return m_Frames.size(); }

    /**
     * @brief Dumps the recorded input states to a text file.
     * @param filePath Path where to save the text dump.
     * @param includePhysics Whether to include physics data in the dump.
     * @return True if the dump was successful.
     */
    bool DumpFrameData(const std::string &filePath, bool includePhysics = false) const;

    /**
     * @brief Loads frame data from a text file.
     * @param filePath Path to the text file to load.
     * @param includePhysics Whether the file contains physics data.
     * @return True if the load was successful.
     */
    bool LoadFrameData(const std::string &filePath, bool includePhysics = false);


    /**
     * @brief Dumps the recorded frame data to a modern binary file.
     * @param filePath Path where to save the binary dump.
     * @return True if the dump was successful.
     */
    bool DumpFrameDataBinary(const std::string &filePath) const;

    /**
     * @brief Loads frame data from a modern binary file.
     * @param filePath Path to the binary file to load.
     * @return True if the load was successful.
     */
    bool LoadFrameDataBinary(const std::string &filePath);

    /**
     * @brief Dumps both text and binary formats with automatic file naming.
     * @param basePath Base path for the files (without extension).
     * @param includePhysics Whether to include physics data in text dump.
     * @return Pair of (text_success, binary_success).
     */
    std::pair<bool, bool> DumpFrameDataBoth(const std::string &basePath, bool includePhysics = false) const;

    /**
     * @brief Clears all recorded frame data.
     */
    void ClearFrameData();

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
     * @brief Sets the generation options to use when auto-generating scripts.
     * @param options The generation options to use.
     */
    void SetGenerationOptions(const GenerationOptions &options);

    /**
     * @brief Gets the current generation options.
     * @return The current generation options.
     */
    const GenerationOptions &GetGenerationOptions() const { return *m_GenerationOptions; }

    /**
     * @brief Sets whether this recording session is part of a translation.
     * This affects metadata and generation options.
     * @param isTranslation True if this is a translation session.
     */
    void SetTranslationMode(bool isTranslation) { m_IsTranslationMode = isTranslation; }

    /**
     * @brief Gets whether this is a translation session.
     * @return True if recording as part of translation.
     */
    bool IsTranslationMode() const { return m_IsTranslationMode; }

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
        m_DeltaTime = 1000.0f / tickPerSecond;
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
     * @param keyboardState The current keyboard state array.
     * @return A RawInputState struct.
     */
    RawInputState CaptureRealInput(const unsigned char *keyboardState) const;

    /**
     * @brief Captures comprehensive physics data for analysis.
     * @param frameData The frame data to populate with physics info.
     */
    void CapturePhysicsData(FrameData &frameData) const;

    /**
     * @brief Notifies UI/callbacks about recording state changes.
     * @param isRecording New recording state.
     */
    void NotifyStatusChange(bool isRecording);

    /**
     * @brief Formats a RawInputState as a human-readable string.
     * @param rawInput The input state to format.
     * @return Formatted string representation.
     */
    static std::string FormatInputStateText(const RawInputState &rawInput);
    /**
     * @brief Parses a text representation back to RawInputState.
     * @param inputText The text representation to parse.
     * @return Parsed RawInputState.
     */
    static RawInputState ParseInputStateText(const std::string &inputText);

    /**
     * @brief Parses a position string like "(x,y,z)" to VxVector.
     * @param posText The position text to parse.
     * @return Parsed VxVector.
     */
    static VxVector ParseVectorText(const std::string &posText);

    /**
     * @brief Trims whitespace from both ends of a string.
     * @param str The string to trim.
     * @return Trimmed string.
     */
    static std::string TrimString(const std::string &str);

    /**
     * @brief Splits a string by delimiter.
     * @param str The string to split.
     * @param delimiter The delimiter character.
     * @return Vector of split parts.
     */
    static std::vector<std::string> SplitString(const std::string &str, char delimiter);

    // Core references
    TASEngine *m_Engine;

    // Recording state
    bool m_IsRecording = false;
    bool m_IsTranslationMode = false;

    // Configuration
    bool m_AutoGenerateOnStop = true; // Auto-generate by default
    float m_DeltaTime = 1000.0f / 132.0f; // Default to 132 FPS
    std::unique_ptr<GenerationOptions> m_GenerationOptions;

    // Recorded data
    std::vector<FrameData> m_Frames;
    std::vector<GameEvent> m_PendingEvents; // Events waiting to be assigned to a frame

    // Callbacks
    std::function<void(bool)> m_StatusCallback;

    // Performance tracking
    size_t m_MaxFrames = 1000000; // Limit to prevent memory issues
    bool m_WarnedMaxFrames = false;

    // Cached remapped keys (acquired once when playback starts)
    CKKEYBOARD m_KeyUp = CKKEY_UP;
    CKKEYBOARD m_KeyDown = CKKEY_DOWN;
    CKKEYBOARD m_KeyLeft = CKKEY_LEFT;
    CKKEYBOARD m_KeyRight = CKKEY_RIGHT;
    CKKEYBOARD m_KeyShift = CKKEY_LSHIFT;
    CKKEYBOARD m_KeySpace = CKKEY_SPACE;
};
