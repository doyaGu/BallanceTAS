#include "ScriptGenerator.h"

#include <set>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <thread>

#include "TASEngine.h"
#include "BallanceTAS.h"
#include "GameInterface.h"

namespace fs = std::filesystem;

// Key name mapping for consistent script generation
const std::vector<std::string> ScriptGenerator::KEY_NAMES = {
    "up", "down", "left", "right", "lshift", "space", "q", "escape"
};

// ===================================================================
// LuaScriptBuilder Implementation
// ===================================================================

ScriptGenerator::LuaScriptBuilder::LuaScriptBuilder(const GenerationOptions &options)
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

void ScriptGenerator::LuaScriptBuilder::AddLine(const std::string &line) {
    m_ss << m_CurrentIndent << line << "\n";
}

void ScriptGenerator::LuaScriptBuilder::AddComment(const std::string &comment) {
    m_ss << m_CurrentIndent << "-- " << comment << "\n";
}

void ScriptGenerator::LuaScriptBuilder::AddBlockComment(const std::string &comment) {
    std::istringstream iss(comment);
    std::string line;
    m_ss << m_CurrentIndent << "--[[\n";
    while (std::getline(iss, line)) {
        m_ss << m_CurrentIndent << "   " << line << "\n";
    }
    m_ss << m_CurrentIndent << "--]]\n";
}

void ScriptGenerator::LuaScriptBuilder::AddBlankLine() {
    m_ss << "\n";
}

void ScriptGenerator::LuaScriptBuilder::AddSeparator(const std::string &title) {
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
    AddLine("tas.log(\"TAS script completed.\")");
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

ScriptGenerator::ScriptGenerator(TASEngine *engine)
    : m_Engine(engine), m_Mod(engine->GetMod()) {
    if (!m_Engine || !m_Mod) {
        throw std::runtime_error("ScriptGenerator requires valid TASEngine and BallanceTAS instances.");
    }
}

std::string ScriptGenerator::FindAvailableProjectName(const std::string &baseName) {
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

void ScriptGenerator::GenerateAsync(const std::vector<FrameData> &frames,
                                    const GenerationOptions &options,
                                    const std::function<void(bool)> &onComplete) {
    std::thread([this, frames, options, onComplete]() {
        bool success = Generate(frames, options);

        // When done, notify the main thread.
        m_Mod->GetBML()->AddTimer(1ul, [this, success, onComplete]() {
            if (onComplete) {
                onComplete(success);
            }
        });
    }).detach();
}

bool ScriptGenerator::Generate(const std::vector<FrameData> &frames, const GenerationOptions &options) {
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

        GenerationOptions finalOptions = options;
        finalOptions.projectName = finalProjectName;

        m_Mod->GetLogger()->Info("Generating TAS script '%s' from %zu frames...",
                                 finalOptions.projectName.c_str(), frames.size());

        // Create project directory
        std::string projectDir = std::string(BML_TAS_PATH) + finalOptions.projectName;
        if (!fs::create_directories(projectDir) && !fs::exists(projectDir)) {
            m_Mod->GetLogger()->Error("Failed to create project directory: %s", projectDir.c_str());
            return false;
        }
        m_LastGeneratedPath = projectDir;
        UpdateProgress(0.1f);

        // Analyze timing
        m_Mod->GetLogger()->Info("Analyzing frame data...");
        auto blocks = AnalyzeTiming(frames, finalOptions);
        m_LastStats.totalBlocks = blocks.size();
        UpdateProgress(0.4f);

        // Generate script
        m_Mod->GetLogger()->Info("Building script...");
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

        m_Mod->GetLogger()->Info("Script generation completed successfully!");
        m_Mod->GetLogger()->Info("  Project: %s", projectDir.c_str());
        m_Mod->GetLogger()->Info("  Blocks: %zu", m_LastStats.totalBlocks);
        m_Mod->GetLogger()->Info("  Key events: %zu", m_LastStats.keyEvents);
        m_Mod->GetLogger()->Info("  Generation time: %.2fs", m_LastStats.generationTime);

        return true;
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Exception during script generation: %s", e.what());
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
            currentBlock.gameEvents.emplace_back(frame.frameIndex, event.eventName, event.eventData);
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

std::vector<KeyEvent> ScriptGenerator::DetectKeyTransitions(const RawInputState &prevState,
                                                            const RawInputState &currentState,
                                                            size_t frameIndex) {
    std::vector<KeyEvent> events;

    for (int keyIdx = 0; keyIdx < KEY_COUNT; ++keyIdx) {
        uint8_t prevKeyState = GetKeyState(prevState, keyIdx);
        uint8_t currentKeyState = GetKeyState(currentState, keyIdx);

        // Skip if no change
        if (prevKeyState == currentKeyState) {
            continue;
        }

        // Analyze the bit flags properly
        bool wasPrevPressed = (prevKeyState & KS_PRESSED) != 0;
        bool isCurrentPressed = (currentKeyState & KS_PRESSED) != 0;
        bool isCurrentReleased = (currentKeyState & KS_RELEASED) != 0;

        KeyTransition transition = KeyTransition::NoChange;

        // Check for the special case: pressed and released in the same frame
        if (isCurrentPressed && isCurrentReleased) {
            // Key was pressed and released in the same frame
            // This happens when the key was pressed and released within a single frame
            transition = KeyTransition::PressedAndReleased;
        } else if (!wasPrevPressed && isCurrentPressed) {
            // Check for key press transition
            // Key went from not-pressed to pressed
            transition = KeyTransition::Pressed;
        } else if (wasPrevPressed && isCurrentReleased) {
            // Check for key release transition
            // Key went from pressed to released
            // Note: this handles both PRESSED -> RELEASED and PRESSED -> IDLE transitions
            transition = KeyTransition::Released;
        }

        // Only add events for meaningful transitions
        if (transition != KeyTransition::NoChange) {
            events.emplace_back(frameIndex, GetKeyName(keyIdx), transition);
        }
    }

    return events;
}

std::string ScriptGenerator::BuildScript(const std::vector<FrameData> &frames,
                                         const std::vector<InputBlock> &blocks,
                                         const GenerationOptions &options) {
    LuaScriptBuilder builder(options);

    // Script header
    builder.AddComment("TAS script for Ballance");
    builder.AddComment("Project: " + options.projectName);
    builder.AddComment("Generated on: " + []() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }());
    builder.AddComment("Total key events: " + std::to_string(m_LastStats.keyEvents));
    builder.AddSeparator();

    builder.AddMainFunction();

    // Track currently pressed keys for validation and cleanup
    std::set<std::string> currentlyPressed;

    // Collect ALL events with frame associations
    std::vector<std::pair<size_t, std::variant<KeyEvent, GameEvent>>> allEvents;

    for (const auto &block : blocks) {
        // Add key events
        for (const auto &keyEvent : block.keyEvents) {
            allEvents.emplace_back(keyEvent.frame, keyEvent);
        }

        // Add game events
        for (const auto &gameEvent : block.gameEvents) {
            allEvents.emplace_back(gameEvent.frame, gameEvent);
        }
    }

    // Sort all events by frame number for chronological processing
    std::sort(allEvents.begin(), allEvents.end(),
              [](const auto &a, const auto &b) {
                  // Handle events at the same frame: game events first, then key events
                  if (a.first == b.first) {
                      bool aIsGame = std::holds_alternative<GameEvent>(a.second);
                      bool bIsGame = std::holds_alternative<GameEvent>(b.second);
                      return aIsGame && !bIsGame; // Game events before key events
                  }
                  return a.first < b.first;
              });

    size_t lastFrame = 0;

    // Wait to the first frame if it's not 0
    if (!allEvents.empty() && allEvents[0].first > 0) {
        size_t initialWait = allEvents[0].first;
        if (options.addFrameComments) {
            builder.AddComment("Wait " + std::to_string(initialWait) + " frames to start");
        }
        builder.AddLine("tas.wait_ticks(" + std::to_string(initialWait) + ")");
        lastFrame = allEvents[0].first;
    }

    // Process all events in chronological order
    for (size_t i = 0; i < allEvents.size(); ++i) {
        const auto &[frameNumber, event] = allEvents[i];

        // Wait until this event's frame
        int64_t waitFrames = frameNumber - lastFrame;
        if (waitFrames > 0) {
            if (options.addFrameComments) {
                builder.AddComment("Wait " + std::to_string(waitFrames) +
                    " frames (to frame " + std::to_string(frameNumber) + ")");
            }
            builder.AddLine("tas.wait_ticks(" + std::to_string(waitFrames) + ")");
        }

        // Handle the event based on its type
        if (std::holds_alternative<KeyEvent>(event)) {
            const auto &keyEvent = std::get<KeyEvent>(event);

            // Generate key command based on transition type
            if (keyEvent.transition == KeyTransition::Pressed) {
                currentlyPressed.insert(keyEvent.key);

                if (options.addFrameComments) {
                    builder.AddComment("Press " + keyEvent.key + " at frame " + std::to_string(keyEvent.frame));
                }
                builder.AddLine("tas.key_down(\"" + keyEvent.key + "\")");
            } else if (keyEvent.transition == KeyTransition::Released) {
                currentlyPressed.erase(keyEvent.key);

                if (options.addFrameComments) {
                    builder.AddComment("Release " + keyEvent.key + " at frame " + std::to_string(keyEvent.frame));
                }
                builder.AddLine("tas.key_up(\"" + keyEvent.key + "\")");
            } else if (keyEvent.transition == KeyTransition::PressedAndReleased) {
                // Key was pressed and released in the same frame
                // Use tas.press() for single-frame press/release
                if (options.addFrameComments) {
                    builder.AddComment(
                        "Press and release " + keyEvent.key + " in single frame " + std::to_string(keyEvent.frame));
                }
                builder.AddLine("tas.press(\"" + keyEvent.key + "\")");

                // Don't track this in currentlyPressed since it's immediately released
            }
        } else if (std::holds_alternative<GameEvent>(event)) {
            const auto &gameEvent = std::get<GameEvent>(event);

            // Game events placed at their exact frame
            if (options.addEventAnchors) {
                builder.AddComment("GAME EVENT: " + gameEvent.eventName +
                    (gameEvent.eventData != 0 ? " (data: " + std::to_string(gameEvent.eventData) + ")" : "") +
                    " at frame " + std::to_string(gameEvent.frame));
            }
        }

        lastFrame = frameNumber;

        // Add section separator every 20 events for readability
        if (options.addSectionSeparators && (i + 1) % 20 == 0 && i + 1 < allEvents.size()) {
            builder.AddBlankLine();
            builder.AddComment("--- Section " + std::to_string((i + 1) / 20 + 1) + " ---");
            builder.AddBlankLine();
        }
    }

    // Wait until the actual end of recording, then release remaining keys
    if (!frames.empty()) {
        // Find the true final frame from the original recording data
        size_t finalRecordingFrame = frames.back().frameIndex;

        // Wait until the final frame if we haven't reached it yet
        int64_t finalWait = finalRecordingFrame - lastFrame;
        if (finalWait > 0) {
            builder.AddBlankLine();
            if (options.addFrameComments) {
                builder.AddComment("Wait until end of recording (frame " + std::to_string(finalRecordingFrame) + ")");
            }
            builder.AddLine("tas.wait_ticks(" + std::to_string(finalWait) + ")");
        }

        // Now release any keys that are still pressed
        if (!currentlyPressed.empty()) {
            builder.AddBlankLine();
            builder.AddComment("Recording ended - release all remaining pressed keys");
            for (const auto &key : currentlyPressed) {
                if (options.addFrameComments) {
                    builder.AddComment("Release " + key + " at end of recording (frame " + std::to_string(finalRecordingFrame) + ")");
                }
                builder.AddLine("tas.key_up(\"" + key + "\")");
            }
        }
    }

    builder.CloseMainFunction();
    return builder.GetScript();
}

std::string ScriptGenerator::GenerateManifest(const GenerationOptions &options) {
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
    ss << "  key_events = " << m_LastStats.keyEvents << ",\n";
    ss << "  total_frames = " << m_LastStats.totalFrames << ",\n";
    ss << "  blocks = " << m_LastStats.totalBlocks << "\n";
    ss << "}\n";

    return ss.str();
}

bool ScriptGenerator::CreateProjectFiles(const std::string &projectPath,
                                         const std::string &scriptContent,
                                         const std::string &manifestContent) {
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
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Exception creating project files: %s", e.what());
        return false;
    }
}

std::string ScriptGenerator::GetKeyName(int keyIndex) const {
    if (keyIndex >= 0 && keyIndex < static_cast<int>(KEY_NAMES.size())) {
        return KEY_NAMES[keyIndex];
    }
    return "unknown";
}

uint8_t ScriptGenerator::GetKeyState(const RawInputState &state, int keyIndex) const {
    switch (keyIndex) {
    case 0: return state.keyUp;
    case 1: return state.keyDown;
    case 2: return state.keyLeft;
    case 3: return state.keyRight;
    case 4: return state.keyShift;
    case 5: return state.keySpace;
    case 6: return state.keyQ;
    case 7: return state.keyEsc;
    default: return KS_IDLE;
    }
}

void ScriptGenerator::UpdateProgress(float progress) {
    if (m_ProgressCallback) {
        try {
            m_ProgressCallback(std::max(0.0f, std::min(1.0f, progress)));
        } catch (const std::exception &e) {
            m_Mod->GetLogger()->Error("Error in progress callback: %s", e.what());
        }
    }
}
