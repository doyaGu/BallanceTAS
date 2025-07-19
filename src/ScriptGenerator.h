#pragma once

#include "Recorder.h"

#include <string>
#include <vector>
#include <sstream>
#include <memory>

// Forward declarations
class TASEngine;
class TASProject;

/**
 * @enum KeyTransition
 * @brief Represents a key state transition between frames.
 */
enum class KeyTransition {
    NoChange,           // Key state didn't change
    Pressed,            // Key was just pressed (IDLE -> PRESSED)
    Released,           // Key was just released (PRESSED -> RELEASED)
    PressedAndReleased, // Key was pressed and then released in the same frame
};

/**
 * @struct KeyEvent
 * @brief Represents a key state change at a specific frame.
 */
struct KeyEvent {
    size_t frame = 0;
    std::string key;
    KeyTransition transition = KeyTransition::NoChange;

    KeyEvent(size_t f, std::string k, KeyTransition t)
        : frame(f), key(std::move(k)), transition(t) {}
};

/**
 * @struct InputBlock
 * @brief A precise representation of input over time.
 *        This tracks exact frame-by-frame changes.
 */
struct InputBlock {
    size_t startFrame = 0;
    size_t endFrame = 0;
    std::vector<KeyEvent> keyEvents;   // All key transitions in this block
    std::vector<GameEvent> gameEvents; // Game events that occurred

    // Analysis metadata
    float averageSpeed = 0.0f;
    bool hasSignificantMovement = false;

    size_t GetDuration() const { return endFrame - startFrame + 1; }
    bool IsEmpty() const { return keyEvents.empty() && endFrame == startFrame; }
};

/**
 * @struct GenerationOptions
 * @brief Configuration options for script generation.
 */
struct GenerationOptions {
    std::string projectName = "Generated_TAS";
    std::string authorName = "Recorder";
    std::string targetLevel = "Level_01";
    std::string description = "Auto-generated TAS script";
    float updateRate = 132.0f; // Default update rate in seconds (132 FPS)

    // Generation preferences
    bool addFrameComments = true;    // Add frame number comments
    bool addPhysicsComments = false; // Add speed/physics info in comments

    // Output formatting
    int indentSize = 2;               // Spaces per indent level
    bool addSectionSeparators = true; // Add visual separators between sections
    bool addEventAnchors = true;      // Add event-based comments
};

/**
 * @class ScriptGenerator
 * @brief Analyzes a sequence of raw frame data and generates a structured Lua script.
 *
 * This class implements precise script generation that captures exact key press/release
 * timing from the recorded input data. It generates explicit tas.key_down() and
 * tas.key_up() commands to exactly reproduce the original input sequence.
 */
class ScriptGenerator {
public:
    explicit ScriptGenerator(TASEngine *engine);
    ~ScriptGenerator() = default;

    // ScriptGenerator is not copyable or movable
    ScriptGenerator(const ScriptGenerator &) = delete;
    ScriptGenerator &operator=(const ScriptGenerator &) = delete;

    /**
     * @brief Asynchronously generates a TAS script from the recorded frames.
     * @param frames The raw frame data captured by the Recorder.
     * @param options Configuration options for generation.
     * @param onComplete Callback called when generation is complete.
     */
    void GenerateAsync(const std::vector<FrameData> &frames,
                       const GenerationOptions &options,
                       const std::function<void(bool)> &onComplete);

    /**
     * @brief The main generation method.
     * @param frames The raw frame data captured by the Recorder.
     * @param options Configuration options for generation.
     * @return True if the script and project were generated successfully.
     */
    bool Generate(const std::vector<FrameData> &frames, const GenerationOptions &options = {});

    /**
     * @brief Get the path of the last generated project.
     * @return Path to the generated project directory.
     */
    std::string GetLastGeneratedPath() const { return m_LastGeneratedPath; }

    /**
     * @brief Set a callback to be called during generation progress.
     * @param callback Function called with progress percentage (0.0 to 1.0).
     */
    void SetProgressCallback(std::function<void(float)> callback) {
        m_ProgressCallback = std::move(callback);
    }

private:
    /**
     * @brief Finds an available project name, handling duplicates by adding numeric suffixes.
     * @param baseName The desired base name for the project.
     * @return An available project name (may have numeric suffix if base name exists).
     */
    std::string FindAvailableProjectName(const std::string &baseName);

    /**
     * @brief Analyzes frame sequence and detects all key state transitions.
     * @param frames The raw frame data.
     * @param options Generation options.
     * @return A vector of InputBlocks.
     */
    std::vector<InputBlock> AnalyzeTiming(const std::vector<FrameData> &frames,
                                          const GenerationOptions &options);

    /**
     * @brief Detects key state transitions between two consecutive frames.
     * @param previousState Previous frame's input state.
     * @param currentState Current frame's input state.
     * @param frameIndex Current frame number.
     * @return Vector of key events for this frame.
     */
    std::vector<KeyEvent> DetectKeyTransitions(const RawInputState &previousState,
                                               const RawInputState &currentState,
                                               size_t frameIndex);

    /**
     * @brief Generates the main script with structure and comments.
     * @param frames The raw frame data.
     * @param blocks The analyzed input blocks.
     * @param options Generation options.
     * @return A string containing the script.
     */
    std::string BuildScript(const std::vector<FrameData> &frames,
                            const std::vector<InputBlock> &blocks,
                            const GenerationOptions &options);

    /**
     * @brief Generate the manifest.lua file for the project.
     * @param options Generation options.
     * @return The manifest content as a string.
     */
    std::string GenerateManifest(const GenerationOptions &options);

    /**
     * @brief Create the project directory and files.
     * @param projectPath The full path to the project directory.
     * @param scriptContent The main.lua content.
     * @param manifestContent The manifest.lua content.
     * @return True if files were created successfully.
     */
    bool CreateProjectFiles(const std::string &projectPath,
                            const std::string &scriptContent,
                            const std::string &manifestContent);

    /**
     * @brief Update progress callback if set.
     * @param progress Progress value from 0.0 to 1.0.
     */
    void UpdateProgress(float progress);

    /**
     * @brief Get the string name for a key from the input state.
     * @param keyIndex Index of the key in the RawInputState structure.
     * @return String name of the key.
     */
    static std::string GetKeyName(int keyIndex);

    /**
     * @brief Get the keyboard state value for a specific key from RawInputState.
     * @param state The input state.
     * @param keyIndex Index of the key.
     * @return The keyboard state value (KS_IDLE, KS_PRESSED, etc.).
     */
    static uint8_t GetKeyState(const RawInputState &state, int keyIndex);

    /**
     * @brief A helper class to build the Lua script string with proper indentation.
     */
    class LuaScriptBuilder {
    public:
        explicit LuaScriptBuilder(const GenerationOptions &options);

        void Indent();
        void Unindent();
        void AddLine(const std::string &line);
        void AddComment(const std::string &comment);
        void AddBlockComment(const std::string &comment);
        void AddBlankLine();
        void AddSeparator(const std::string &title = "");
        void AddMainFunction();
        void CloseMainFunction();

        std::string GetScript() const;

    private:
        std::stringstream m_SS;
        int m_IndentLevel = 0;
        std::string m_CurrentIndent;
        const GenerationOptions &m_Options;
        bool m_InMainFunction = false;
    };

    // Core references
    TASEngine *m_Engine;

    // State
    std::string m_LastGeneratedPath;
    std::function<void(float)> m_ProgressCallback;

    // Statistics
    struct GenerationStats {
        size_t totalFrames = 0;
        size_t totalBlocks = 0;
        size_t keyEvents = 0;
        size_t eventsProcessed = 0;
        double generationTime = 0.0;
    } m_LastStats;

    // Key constants
    static const std::vector<std::string> KEY_NAMES;
    static constexpr int KEY_COUNT = 8;
};
