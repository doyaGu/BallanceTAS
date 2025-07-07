#pragma once

#include "Recorder.h"

#include <string>
#include <vector>
#include <set>
#include <map>
#include <sstream>
#include <memory>

// Forward declarations
class TASProject;
class TASEngine;
class BallanceTAS;

/**
 * @struct InputBlock
 * @brief An intermediate representation of a contiguous block of frames
 *        where the combination of pressed keys remains constant.
 */
struct InputBlock {
    int startFrame = 0;
    int duration = 0;               // Number of frames this block lasts
    std::set<std::string> heldKeys; // The set of keys held down during this block
    std::vector<GameEvent> events;  // Events that occurred during this block

    // Analysis metadata
    float averageSpeed = 0.0f;           // Average ball speed during this block
    bool hasSignificantMovement = false; // Whether this block represents meaningful action

    bool IsEmpty() const {
        return heldKeys.empty() && duration <= 0;
    }

    bool HasSingleKey() const {
        return heldKeys.size() == 1;
    }

    std::string GetSingleKey() const {
        return HasSingleKey() ? *heldKeys.begin() : "";
    }
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

    // Generation preferences
    bool optimizeShortWaits = true;  // Merge short wait periods
    bool addFrameComments = true;    // Add frame number comments
    bool addPhysicsComments = false; // Add speed/physics info in comments
    bool groupSimilarActions = true; // Group similar input patterns
    int minBlockDuration = 1;        // Minimum frames for a block to be significant

    // Output formatting
    int indentSize = 2;               // Spaces per indent level
    bool addSectionSeparators = true; // Add visual separators between sections
    bool addEventAnchors = true;      // Add event-based comments
};

/**
 * @class ScriptGenerator
 * @brief Analyzes a sequence of raw frame data and generates a structured Lua script.
 *
 * This class implements the intelligent script generation algorithm. It takes the
 * raw, frame-by-frame data from the Recorder, analyzes it to find patterns and
 * logical blocks of actions, and then uses this analysis to generate a clean,
 * readable, and efficient Lua script using the `tas` API.
 */
class ScriptGenerator {
public:
    explicit ScriptGenerator(TASEngine *engine);
    ~ScriptGenerator() = default;

    // ScriptGenerator is not copyable or movable
    ScriptGenerator(const ScriptGenerator &) = delete;
    ScriptGenerator &operator=(const ScriptGenerator &) = delete;

    /**
     * @brief The main generation method.
     * @param frames The raw frame data captured by the Recorder.
     * @param options Configuration options for generation.
     * @return True if the script and project were generated successfully.
     */
    bool Generate(const std::vector<RawFrameData> &frames, const GenerationOptions &options = {});

    /**
     * @brief Generate with simple project name (uses default options).
     * @param frames The raw frame data.
     * @param projectName The name for the new TAS project.
     * @return True if generation was successful.
     */
    bool Generate(const std::vector<RawFrameData> &frames, const std::string &projectName);

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
     * @brief The first pass of the algorithm: analyze the frame sequence and
     *        group it into logical InputBlocks.
     * @param frames The raw frame data.
     * @param options Generation options.
     * @return A vector of InputBlocks.
     */
    std::vector<InputBlock> AnalyzeAndChunk(const std::vector<RawFrameData> &frames,
                                            const GenerationOptions &options);

    /**
     * @brief Optimize the input blocks by merging similar patterns.
     * @param blocks The initial blocks to optimize.
     * @param options Generation options.
     * @return Optimized blocks.
     */
    std::vector<InputBlock> OptimizeBlocks(const std::vector<InputBlock> &blocks,
                                           const GenerationOptions &options);

    /**
     * @brief The second pass: iterate through the blocks and generate Lua code for each.
     * @param blocks The chunked input blocks.
     * @param options Generation options.
     * @return A string containing the full, formatted Lua script.
     */
    std::string BuildLuaScript(const std::vector<InputBlock> &blocks,
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
     * @brief Convert RawInputState to a set of key strings.
     * @param state The input state to convert.
     * @return Set of key names that are pressed.
     */
    std::set<std::string> GetHeldKeys(const RawInputState &state);

    /**
     * @brief Update progress callback if set.
     * @param progress Progress value from 0.0 to 1.0.
     */
    void UpdateProgress(float progress);

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
        void AddBlankLine();
        void AddSeparator(const std::string &title = "");
        void AddMainFunction();
        void CloseMainFunction();

        std::string GetScript() const;

    private:
        std::stringstream m_ss;
        int m_IndentLevel = 0;
        std::string m_CurrentIndent;
        const GenerationOptions &m_Options;
        bool m_InMainFunction = false;
    };

    // Core references
    TASEngine *m_Engine;
    BallanceTAS *m_Mod;

    // State
    std::string m_LastGeneratedPath;
    std::function<void(float)> m_ProgressCallback;

    // Statistics
    struct GenerationStats {
        size_t totalFrames = 0;
        size_t totalBlocks = 0;
        size_t optimizedBlocks = 0;
        size_t eventsProcessed = 0;
        double generationTime = 0.0;
    } m_LastStats;
};
