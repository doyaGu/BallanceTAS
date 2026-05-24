#include "ScriptGenerator.h"

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <thread>

#include "Logger.h"
#include "TASEngine.h"
#include "GameInterface.h"
#include "ScriptGenerationCore.h"

namespace fs = std::filesystem;

// ===================================================================
// ScriptGenerator Implementation
// ===================================================================

ScriptGenerator::ScriptGenerator(TASEngine *engine) : m_Engine(engine) {
    if (!m_Engine) {
        throw std::runtime_error("ScriptGenerator requires valid TASEngine instances.");
    }
}

std::string ScriptGenerator::FindAvailableProjectName(const std::string &baseName) {
    std::string projectDir = m_Engine->GetPath() + baseName;

    // If the base name doesn't exist, use it
    if (!fs::exists(projectDir)) {
        return baseName;
    }

    // Try incrementing numbers until we find an available name
    int counter = 1;
    std::string availableName;

    do {
        availableName = baseName + "_" + std::to_string(counter);
        projectDir = m_Engine->GetPath() + availableName;
        counter++;

        // Safety check to avoid infinite loop
        if (counter > 1000) {
            Log::Error("Could not find available project name after 1000 attempts.");
            return baseName + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        }
    } while (fs::exists(projectDir));

    return availableName;
}

void ScriptGenerator::GenerateAsync(const std::vector<FrameData> &frames,
                                    const GenerationOptions &options,
                                    const std::function<void(bool)> &onComplete) {
    std::thread([this, frames, options, onComplete]() {
        bool success = Generate(frames, options);

        // When done, notify the main thread.
        m_Engine->AddTimer(1ul, [success, onComplete]() {
            if (onComplete) {
                onComplete(success);
            }
        });
    }).detach();
}

bool ScriptGenerator::Generate(const std::vector<FrameData> &frames, const GenerationOptions &options) {
    if (frames.empty()) {
        Log::Error("Cannot generate script from empty frame data.");
        return false;
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    m_LastStats = {};
    m_LastStats.totalFrames = frames.size();

    try {
        UpdateProgress(0.0f);

        // Handle duplicate project names by finding an available name
        std::string finalProjectName = FindAvailableProjectName(options.projectName);
        if (finalProjectName != options.projectName) {
            Log::Info("Project name '%s' already exists, using '%s' instead.",
                                        options.projectName.c_str(), finalProjectName.c_str());
        }

        GenerationOptions finalOptions = options;
        finalOptions.projectName = finalProjectName;

        Log::Info("Generating TAS script '%s' from %zu frames...",
                                    finalOptions.projectName.c_str(), frames.size());

        // Create project directory
        std::string projectDir = m_Engine->GetPath() + finalOptions.projectName;
        if (!fs::create_directories(projectDir) && !fs::exists(projectDir)) {
            Log::Error("Failed to create project directory: %s", projectDir.c_str());
            return false;
        }
        m_LastGeneratedPath = projectDir;
        UpdateProgress(0.1f);

        // Analyze timing
        Log::Info("Analyzing frame data...");
        auto blocks = AnalyzeTiming(frames, finalOptions);
        m_LastStats.totalBlocks = blocks.size();
        UpdateProgress(0.4f);

        // Generate script
        Log::Info("Building script...");
        std::string scriptContent = BuildScript(frames, blocks, finalOptions);
        UpdateProgress(0.7f);

        // Generate manifest
        std::string manifestContent = GenerateManifest(finalOptions);
        UpdateProgress(0.9f);

        // Write files
        if (!CreateProjectFiles(projectDir, scriptContent, manifestContent)) {
            return false;
        }
        UpdateProgress(1.0f);

        auto endTime = std::chrono::high_resolution_clock::now();
        m_LastStats.generationTime = std::chrono::duration<double>(endTime - startTime).count();

        Log::Info("Script generation completed successfully!");
        Log::Info("  Project: %s", projectDir.c_str());
        Log::Info("  Blocks: %zu", m_LastStats.totalBlocks);
        Log::Info("  Key events: %zu", m_LastStats.keyEvents);
        Log::Info("  Generation time: %.2fs", m_LastStats.generationTime);

        return true;
    } catch (const std::exception &e) {
        Log::Error("Exception during script generation: %s", e.what());
        return false;
    }
}

std::vector<InputBlock> ScriptGenerator::AnalyzeTiming(const std::vector<FrameData> &frames,
                                                       const GenerationOptions &options) {
    std::vector<InputBlock> blocks;
    if (frames.empty()) return blocks;

    InputBlock currentBlock;
    currentBlock.startFrame = frames[0].frameIndex;
    currentBlock.endFrame = frames[0].frameIndex;

    RawInputState previousState; // Start with all keys idle
    float totalSpeed = 0.0f;
    int speedSamples = 0;

    for (size_t i = 0; i < frames.size(); ++i) {
        const auto &frame = frames[i];

        // Detect key transitions
        auto keyEvents = DetectKeyTransitions(previousState, frame.inputState, frame.frameIndex);

        // Add key events to current block
        currentBlock.keyEvents.insert(currentBlock.keyEvents.end(), keyEvents.begin(), keyEvents.end());
        m_LastStats.keyEvents += keyEvents.size();

        // Properly preserve frame association for game events
        for (const auto &event : frame.events) {
            GameEvent copiedEvent = event;
            copiedEvent.frame = frame.frameIndex;
            currentBlock.gameEvents.push_back(copiedEvent);
            m_LastStats.eventsProcessed++;
        }

        // Track physics data
        if (frame.physics.speed > 0.0f) {
            totalSpeed += frame.physics.speed;
            speedSamples++;
        }

        // Update block end frame BEFORE checking for splits
        currentBlock.endFrame = frame.frameIndex;

        // Check if we should start a new block (but not on the last frame)
        bool shouldStartNewBlock = false;
        if (i < frames.size() - 1 && options.addSectionSeparators) {
            // Start new block when we have accumulated enough events
            size_t totalEvents = currentBlock.keyEvents.size() + currentBlock.gameEvents.size();
            shouldStartNewBlock = (totalEvents > 25); // Adjustable threshold

            // Also split on significant time gaps (optional)
            if (!shouldStartNewBlock && i + 1 < frames.size()) {
                size_t frameGap = frames[i + 1].frameIndex - frame.frameIndex;
                shouldStartNewBlock = (frameGap > 30); // Split on gaps > 30 frames
            }
        }

        if (shouldStartNewBlock) {
            // Finalize current block
            if (speedSamples > 0) {
                currentBlock.averageSpeed = totalSpeed / speedSamples;
                currentBlock.hasSignificantMovement = currentBlock.averageSpeed > 1.0f;
            }

            if (!currentBlock.IsEmpty()) {
                blocks.push_back(currentBlock);
            }

            // Start new block from next frame
            currentBlock = InputBlock{};
            currentBlock.startFrame = frames[i + 1].frameIndex;
            currentBlock.endFrame = frames[i + 1].frameIndex;
            totalSpeed = 0.0f;
            speedSamples = 0;
        }

        previousState = frame.inputState;
    }

    // Add the final block
    if (!currentBlock.IsEmpty()) {
        if (speedSamples > 0) {
            currentBlock.averageSpeed = totalSpeed / speedSamples;
            currentBlock.hasSignificantMovement = currentBlock.averageSpeed > 1.0f;
        }
        blocks.push_back(currentBlock);
    }

    return blocks;
}

std::vector<KeyEvent> ScriptGenerator::DetectKeyTransitions(const RawInputState &previousState,
                                                            const RawInputState &currentState,
                                                            size_t frameIndex) {
    return DetectScriptKeyTransitions(previousState, currentState, frameIndex);
}

std::string ScriptGenerator::BuildScript(const std::vector<FrameData> &frames,
                                         const std::vector<InputBlock> &blocks,
                                         const GenerationOptions &options) {
    ScriptGenerationStatsView stats;
    stats.keyEvents = m_LastStats.keyEvents;
    return BuildGeneratedLuaScript(frames, blocks, options, stats);
}

std::string ScriptGenerator::GenerateManifest(const GenerationOptions &options) {
    ScriptGenerationStatsView stats;
    stats.totalFrames = m_LastStats.totalFrames;
    stats.totalBlocks = m_LastStats.totalBlocks;
    stats.keyEvents = m_LastStats.keyEvents;
    return BuildGeneratedManifest(options, stats);
}

bool ScriptGenerator::CreateProjectFiles(const std::string &projectPath,
                                         const std::string &scriptContent,
                                         const std::string &manifestContent) {
    try {
        // Write main.lua
        std::string scriptPath = projectPath + "/main.lua";
        std::ofstream scriptFile(scriptPath);
        if (!scriptFile.is_open()) {
            Log::Error("Failed to create script file: %s", scriptPath.c_str());
            return false;
        }
        scriptFile << scriptContent;
        scriptFile.close();

        // Write manifest.lua
        std::string manifestPath = projectPath + "/manifest.lua";
        std::ofstream manifestFile(manifestPath);
        if (!manifestFile.is_open()) {
            Log::Error("Failed to create manifest file: %s", manifestPath.c_str());
            return false;
        }
        manifestFile << manifestContent;
        manifestFile.close();

        return true;
    } catch (const std::exception &e) {
        Log::Error("Exception creating project files: %s", e.what());
        return false;
    }
}

void ScriptGenerator::UpdateProgress(float progress) {
    if (m_ProgressCallback) {
        try {
            m_ProgressCallback((std::max)(0.0f, (std::min)(1.0f, progress)));
        } catch (const std::exception &e) {
            Log::Error("Error in progress callback: %s", e.what());
        }
    }
}
