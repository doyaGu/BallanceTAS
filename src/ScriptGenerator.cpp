#include "ScriptGenerator.h"

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <iomanip>

#include "TASEngine.h"
#include "BallanceTAS.h"
#include "GameInterface.h"

namespace fs = std::filesystem;

// ===================================================================
// LuaScriptBuilder Implementation
// ===================================================================

ScriptGenerator::LuaScriptBuilder::LuaScriptBuilder(const GenerationOptions& options)
    : m_Options(options) {
    m_CurrentIndent = std::string(m_IndentLevel * m_Options.indentSize, ' ');
}

void ScriptGenerator::LuaScriptBuilder::Indent() {
    m_IndentLevel++;
    m_CurrentIndent = std::string(m_IndentLevel * m_Options.indentSize, ' ');
}

void ScriptGenerator::LuaScriptBuilder::Unindent() {
    if (m_IndentLevel > 0) {
        m_IndentLevel--;
        m_CurrentIndent = std::string(m_IndentLevel * m_Options.indentSize, ' ');
    }
}

void ScriptGenerator::LuaScriptBuilder::AddLine(const std::string& line) {
    m_ss << m_CurrentIndent << line << "\n";
}

void ScriptGenerator::LuaScriptBuilder::AddComment(const std::string& comment) {
    m_ss << m_CurrentIndent << "-- " << comment << "\n";
}

void ScriptGenerator::LuaScriptBuilder::AddBlankLine() {
    m_ss << "\n";
}

void ScriptGenerator::LuaScriptBuilder::AddSeparator(const std::string& title) {
    if (!m_Options.addSectionSeparators) return;

    AddBlankLine();
    AddComment(std::string(60, '='));
    if (!title.empty()) {
        AddComment(title);
        AddComment(std::string(60, '='));
    }
    AddBlankLine();
}

void ScriptGenerator::LuaScriptBuilder::AddMainFunction() {
    if (m_InMainFunction) return;

    AddComment("Main TAS function - called when the script starts");
    AddLine("function main()");
    Indent();
    m_InMainFunction = true;
}

void ScriptGenerator::LuaScriptBuilder::CloseMainFunction() {
    if (!m_InMainFunction) return;

    AddBlankLine();
    AddComment("Script completed successfully");
    AddLine("tas.log(\"Generated TAS script completed.\")");
    Unindent();
    AddLine("end");
    m_InMainFunction = false;
}

std::string ScriptGenerator::LuaScriptBuilder::GetScript() const {
    return m_ss.str();
}

// ===================================================================
// ScriptGenerator Main Implementation
// ===================================================================

ScriptGenerator::ScriptGenerator(TASEngine* engine)
    : m_Engine(engine), m_Mod(engine->GetMod()) {

    if (!m_Engine || !m_Mod) {
        throw std::runtime_error("ScriptGenerator requires valid TASEngine and BallanceTAS instances.");
    }
}

bool ScriptGenerator::Generate(const std::vector<RawFrameData>& frames, const std::string& projectName) {
    GenerationOptions options;
    options.projectName = projectName;
    return Generate(frames, options);
}

std::string ScriptGenerator::FindAvailableProjectName(const std::string& baseName) {
    std::string projectDir = std::string(BML_TAS_PATH) + baseName;

    // If the base name doesn't exist, use it
    if (!fs::exists(projectDir)) {
        return baseName;
    }

    // Try incrementing numbers until we find an available name
    int counter = 1;
    std::string availableName;

    do {
        availableName = baseName + "_" + std::to_string(counter);
        projectDir = std::string(BML_TAS_PATH) + availableName;
        counter++;

        // Safety check to avoid infinite loop
        if (counter > 1000) {
            m_Mod->GetLogger()->Error("Could not find available project name after 1000 attempts.");
            return baseName + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        }
    } while (fs::exists(projectDir));

    return availableName;
}

bool ScriptGenerator::Generate(const std::vector<RawFrameData>& frames, const GenerationOptions& options) {
    if (frames.empty()) {
        m_Mod->GetLogger()->Error("Cannot generate script from empty frame data.");
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
            m_Mod->GetLogger()->Info("Project name '%s' already exists, using '%s' instead.",
                                   options.projectName.c_str(), finalProjectName.c_str());
        }

        // Create a copy of options with the final project name
        GenerationOptions finalOptions = options;
        finalOptions.projectName = finalProjectName;

        m_Mod->GetLogger()->Info("Generating TAS script '%s' from %zu frames...",
                                finalOptions.projectName.c_str(), frames.size());

        // --- Step 1: Create project directory ---
        std::string projectDir = std::string(BML_TAS_PATH) + finalOptions.projectName;
        if (!fs::create_directories(projectDir) && !fs::exists(projectDir)) {
            m_Mod->GetLogger()->Error("Failed to create project directory: %s", projectDir.c_str());
            return false;
        }
        m_LastGeneratedPath = projectDir;
        UpdateProgress(0.1f);

        // --- Step 2: Analyze and chunk the frame data ---
        m_Mod->GetLogger()->Info("Analyzing frame data...");
        auto blocks = AnalyzeAndChunk(frames, finalOptions);
        m_LastStats.totalBlocks = blocks.size();
        UpdateProgress(0.3f);

        // --- Step 3: Optimize blocks ---
        if (finalOptions.groupSimilarActions) {
            m_Mod->GetLogger()->Info("Optimizing input blocks...");
            blocks = OptimizeBlocks(blocks, finalOptions);
            m_LastStats.optimizedBlocks = blocks.size();
        }
        UpdateProgress(0.5f);

        // --- Step 4: Generate Lua script ---
        m_Mod->GetLogger()->Info("Building Lua script...");
        std::string scriptContent = BuildLuaScript(blocks, finalOptions);
        UpdateProgress(0.7f);

        // --- Step 5: Generate manifest ---
        std::string manifestContent = GenerateManifest(finalOptions);
        UpdateProgress(0.8f);

        // --- Step 6: Write files ---
        if (!CreateProjectFiles(projectDir, scriptContent, manifestContent)) {
            return false;
        }
        UpdateProgress(1.0f);

        auto endTime = std::chrono::high_resolution_clock::now();
        m_LastStats.generationTime = std::chrono::duration<double>(endTime - startTime).count();

        m_Mod->GetLogger()->Info("Script generation completed successfully!");
        m_Mod->GetLogger()->Info("  Project: %s", projectDir.c_str());
        m_Mod->GetLogger()->Info("  Frames: %zu -> %zu blocks", m_LastStats.totalFrames, m_LastStats.optimizedBlocks);
        m_Mod->GetLogger()->Info("  Time: %.2fs", m_LastStats.generationTime);

        return true;

    } catch (const std::exception& e) {
        m_Mod->GetLogger()->Error("Exception during script generation: %s", e.what());
        return false;
    }
}

std::vector<InputBlock> ScriptGenerator::AnalyzeAndChunk(const std::vector<RawFrameData>& frames,
                                                        const GenerationOptions& options) {
    std::vector<InputBlock> blocks;
    if (frames.empty()) return blocks;

    InputBlock currentBlock;
    currentBlock.startFrame = frames[0].frameIndex;
    currentBlock.duration = 0;
    currentBlock.heldKeys = GetHeldKeys(frames[0].inputState);

    float totalSpeed = 0.0f;
    int speedSamples = 0;

    for (const auto& frame : frames) {
        auto frameKeys = GetHeldKeys(frame.inputState);

        // Track physics data
        if (frame.ballSpeed > 0.0f) {
            totalSpeed += frame.ballSpeed;
            speedSamples++;
        }

        // Add events to current block
        for (const auto& event : frame.events) {
            currentBlock.events.push_back(event);
            m_LastStats.eventsProcessed++;
        }

        if (frameKeys == currentBlock.heldKeys) {
            // Keys are the same, extend the current block
            currentBlock.duration++;
        } else {
            // Keys changed, finalize the previous block and start a new one
            if (currentBlock.duration >= options.minBlockDuration) {
                // Calculate average speed for this block
                if (speedSamples > 0) {
                    currentBlock.averageSpeed = totalSpeed / speedSamples;
                    currentBlock.hasSignificantMovement = currentBlock.averageSpeed > 1.0f;
                }
                blocks.push_back(currentBlock);
            }

            // Start new block
            currentBlock = {};
            currentBlock.startFrame = frame.frameIndex;
            currentBlock.duration = 1;
            currentBlock.heldKeys = frameKeys;
            totalSpeed = frame.ballSpeed;
            speedSamples = frame.ballSpeed > 0.0f ? 1 : 0;
        }
    }

    // Add the last block
    if (currentBlock.duration >= options.minBlockDuration) {
        if (speedSamples > 0) {
            currentBlock.averageSpeed = totalSpeed / speedSamples;
            currentBlock.hasSignificantMovement = currentBlock.averageSpeed > 1.0f;
        }
        blocks.push_back(currentBlock);
    }

    return blocks;
}

std::vector<InputBlock> ScriptGenerator::OptimizeBlocks(const std::vector<InputBlock>& blocks,
                                                       const GenerationOptions& options) {
    if (!options.optimizeShortWaits && !options.groupSimilarActions) {
        return blocks; // No optimization requested
    }

    std::vector<InputBlock> optimized;
    optimized.reserve(blocks.size());

    for (size_t i = 0; i < blocks.size(); ++i) {
        const auto& block = blocks[i];

        // Merge short waits with adjacent blocks
        if (options.optimizeShortWaits && block.heldKeys.empty() && block.duration <= 3) {
            // Skip very short wait periods, they'll be absorbed by adjacent blocks
            continue;
        }

        // Group similar actions
        if (options.groupSimilarActions && !optimized.empty()) {
            auto& lastBlock = optimized.back();

            // Can we merge with the previous block?
            if (lastBlock.heldKeys == block.heldKeys &&
                lastBlock.events.empty() && block.events.empty() &&
                (block.startFrame - (lastBlock.startFrame + lastBlock.duration)) <= 2) {

                // Merge blocks
                lastBlock.duration += block.duration + (block.startFrame - (lastBlock.startFrame + lastBlock.duration));
                lastBlock.averageSpeed = (lastBlock.averageSpeed + block.averageSpeed) / 2.0f;
                lastBlock.hasSignificantMovement = lastBlock.hasSignificantMovement || block.hasSignificantMovement;
                continue;
            }
        }

        optimized.push_back(block);
    }

    return optimized;
}

std::string ScriptGenerator::BuildLuaScript(const std::vector<InputBlock>& blocks,
                                           const GenerationOptions& options) {
    LuaScriptBuilder builder(options);

    // Script header
    builder.AddComment("Auto-generated TAS script for Ballance");
    builder.AddComment("Project: " + options.projectName);
    builder.AddComment("Generated on: " + []() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }());
    builder.AddComment("Total blocks: " + std::to_string(blocks.size()));
    builder.AddSeparator();

    builder.AddMainFunction();

    int blockCounter = 0;
    for (const auto& block : blocks) {
        blockCounter++;
        int endFrame = block.startFrame + block.duration - 1;

        if (options.addSectionSeparators && blockCounter > 1) {
            builder.AddBlankLine();
        }

        if (options.addFrameComments) {
            builder.AddComment("Block " + std::to_string(blockCounter) + ": Frames " +
                             std::to_string(block.startFrame) + "-" + std::to_string(endFrame) +
                             " (" + std::to_string(block.duration) + " frames)");
        }

        if (options.addPhysicsComments && block.hasSignificantMovement) {
            builder.AddComment("Average speed: " + std::to_string(block.averageSpeed));
        }

        // Generate code based on block type
        if (block.heldKeys.empty()) {
            // No keys pressed - wait
            if (block.duration > 0) {
                builder.AddLine("tas.wait_ticks(" + std::to_string(block.duration) + ")");
            }
        } else if (block.HasSingleKey()) {
            // Single key
            const std::string& key = block.GetSingleKey();
            if (block.duration == 1) {
                builder.AddLine("tas.press(\"" + key + "\")");
                builder.AddLine("tas.wait_ticks(1)");
            } else {
                builder.AddLine("tas.hold(\"" + key + "\", " + std::to_string(block.duration) + ")");
            }
        } else {
            // Multiple keys - join them with spaces for clean, simple syntax
            std::string combinedKeys;
            bool first = true;
            for (const auto& key : block.heldKeys) {
                if (!first) combinedKeys += " ";
                combinedKeys += key;
                first = false;
            }

            if (options.addFrameComments) {
                builder.AddComment("Multiple keys: " + combinedKeys);
            }

            if (block.duration == 1) {
                builder.AddLine("tas.press(\"" + combinedKeys + "\")");
                builder.AddLine("tas.wait_ticks(1)");
            } else {
                builder.AddLine("tas.hold(\"" + combinedKeys + "\", " + std::to_string(block.duration) + ")");
            }
        }

        // Add event comments/anchors
        if (options.addEventAnchors && !block.events.empty()) {
            builder.AddBlankLine();
            for (const auto& event : block.events) {
                if (event.eventData != 0) {
                    builder.AddComment("Event: " + event.eventName + " (data: " + std::to_string(event.eventData) +
                                     ") at frame ~" + std::to_string(endFrame));
                } else {
                    builder.AddComment("Event: " + event.eventName + " at frame ~" + std::to_string(endFrame));
                }
            }
        }
    }

    builder.CloseMainFunction();
    return builder.GetScript();
}

std::string ScriptGenerator::GenerateManifest(const GenerationOptions& options) {
    std::stringstream ss;

    ss << "-- Auto-generated manifest for " << options.projectName << "\n";
    ss << "return {\n";
    ss << "  name = \"" << options.projectName << "\",\n";
    ss << "  author = \"" << options.authorName << "\",\n";
    ss << "  level = \"" << options.targetLevel << "\",\n";
    ss << "  entry_script = \"main.lua\",\n";
    ss << "  description = \"" << options.description << "\",\n";
    ss << "  update_rate = 132, -- Standard Ballance physics rate\n";
    ss << "  \n";
    ss << "  -- Generation metadata\n";
    ss << "  generated_by = \"BallanceTAS ScriptGenerator\",\n";
    ss << "  generation_date = \"" << []() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
        return ss.str();
    }() << "\",\n";
    ss << "}\n";

    return ss.str();
}

bool ScriptGenerator::CreateProjectFiles(const std::string& projectPath,
                                        const std::string& scriptContent,
                                        const std::string& manifestContent) {
    try {
        // Write main.lua
        std::ofstream scriptFile(projectPath + "/main.lua");
        if (!scriptFile.is_open()) {
            m_Mod->GetLogger()->Error("Failed to create main.lua file.");
            return false;
        }
        scriptFile << scriptContent;
        scriptFile.close();

        // Write manifest.lua
        std::ofstream manifestFile(projectPath + "/manifest.lua");
        if (!manifestFile.is_open()) {
            m_Mod->GetLogger()->Error("Failed to create manifest.lua file.");
            return false;
        }
        manifestFile << manifestContent;
        manifestFile.close();

        return true;

    } catch (const std::exception& e) {
        m_Mod->GetLogger()->Error("Exception creating project files: %s", e.what());
        return false;
    }
}

std::set<std::string> ScriptGenerator::GetHeldKeys(const RawInputState& state) {
    std::set<std::string> keys;

    if (state.keyUp) keys.insert("up");
    if (state.keyDown) keys.insert("down");
    if (state.keyLeft) keys.insert("left");
    if (state.keyRight) keys.insert("right");
    if (state.keyShift) keys.insert("lshift");
    if (state.keySpace) keys.insert("space");
    if (state.keyQ) keys.insert("q");
    if (state.keyEsc) keys.insert("escape");

    return keys;
}

void ScriptGenerator::UpdateProgress(float progress) {
    if (m_ProgressCallback) {
        try {
            m_ProgressCallback(std::max(0.0f, std::min(1.0f, progress)));
        } catch (const std::exception& e) {
            m_Mod->GetLogger()->Error("Error in progress callback: %s", e.what());
        }
    }
}