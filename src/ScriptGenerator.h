#pragma once

#include <string>
#include <vector>

#include "Recorder.h"
#include "ScriptInputTransition.h"

// Forward declarations
class TASEngine;
class TASProject;

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
    struct GenerationStats {
        size_t totalFrames = 0;
        size_t totalBlocks = 0;
        size_t keyEvents = 0;
        size_t eventsProcessed = 0;
        double generationTime = 0.0;
    };

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

    // Core references
    TASEngine *m_Engine;

    // State
    std::string m_LastGeneratedPath;
    std::function<void(float)> m_ProgressCallback;

    // Statistics
    GenerationStats m_LastStats;

};
