#include "Recorder.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "TASEngine.h"
#include "GameInterface.h"
#include "ProjectManager.h"
#include "ScriptGenerator.h"

Recorder::Recorder(TASEngine *engine)
    : m_Engine(engine) {
    if (!m_Engine) {
        throw std::runtime_error("Recorder requires valid TASEngine, BallanceTAS, and IBML instances.");
    }

    // Reserve space for better performance
    m_Frames.reserve(10000);
    m_PendingEvents.reserve(100);

    // Initialize default generation options
    m_GenerationOptions = std::make_unique<GenerationOptions>();
    m_GenerationOptions->projectName = "Generated_TAS";
    m_GenerationOptions->authorName = "Player";
    m_GenerationOptions->targetLevel = "Level_01";
    m_GenerationOptions->description = "Auto-generated TAS script";
    m_GenerationOptions->addFrameComments = true;
    m_GenerationOptions->addPhysicsComments = false;
}

void Recorder::SetGenerationOptions(const GenerationOptions &options) {
    *m_GenerationOptions = options;
}

void Recorder::Start() {
    if (m_IsRecording) {
        m_Engine->GetLogger()->Warn("Recorder is already recording. Stopping previous session.");
        Stop();
    }

    // Clear previous data
    m_Frames.clear();
    m_PendingEvents.clear();
    m_WarnedMaxFrames = false;

    // Acquire remapped keys from game interface
    auto *gameInterface = m_Engine->GetGameInterface();
    if (gameInterface) {
        m_KeyUp = gameInterface->RemapKey(CKKEY_UP);
        m_KeyDown = gameInterface->RemapKey(CKKEY_DOWN);
        m_KeyLeft = gameInterface->RemapKey(CKKEY_LEFT);
        m_KeyRight = gameInterface->RemapKey(CKKEY_RIGHT);
        m_KeyShift = gameInterface->RemapKey(CKKEY_LSHIFT);
        m_KeySpace = gameInterface->RemapKey(CKKEY_SPACE);
    }

    m_IsRecording = true;
    NotifyStatusChange(true);

    const char *modeStr = m_IsTranslationMode ? "translation" : "recording";
    m_Engine->GetLogger()->Info("Started %s session.", modeStr);
}

std::vector<FrameData> Recorder::Stop() {
    if (!m_IsRecording) {
        m_Engine->GetLogger()->Warn("Recorder is not currently recording.");
        return {};
    }

    m_IsRecording = false;
    NotifyStatusChange(false);

    // Process any remaining pending events
    if (!m_PendingEvents.empty() && !m_Frames.empty()) {
        // Assign pending events to the last frame
        m_Frames.back().events.insert(
            m_Frames.back().events.end(),
            m_PendingEvents.begin(),
            m_PendingEvents.end()
        );
        m_PendingEvents.clear();
    }

    // Auto-generate script if we have frames
    if (!m_Frames.empty() && m_AutoGenerateOnStop) {
        GenerateScript();
    }

    // Return a copy of the recorded frames
    return m_Frames;
}

std::string Recorder::GenerateAutoProjectName() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "TAS_" << std::put_time(std::localtime(&time_t), "%y%m%d_%H%M%S");

    return ss.str();
}

bool Recorder::GenerateScript() {
    if (m_Frames.empty()) {
        m_Engine->GetLogger()->Warn("No frames recorded, cannot generate script.");
        return false;
    }

    try {
        // Get script generator from engine
        auto *scriptGenerator = m_Engine->GetScriptGenerator();
        if (!scriptGenerator) {
            m_Engine->GetLogger()->Error("ScriptGenerator not available.");
            return false;
        }

        // Use the stored generation options, but update dynamic fields
        GenerationOptions options = *m_GenerationOptions;

        // If project name is empty, generate auto name
        if (options.projectName.empty()) {
            options.projectName = GenerateAutoProjectName();
        }

        // Clean up the project name (replace invalid characters)
        std::replace_if(options.projectName.begin(), options.projectName.end(),
                        [](char c) {
                            return c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' ||
                                   c == '?' || c == '"' || c == '<' || c == '>' || c == '|';
                        },
                        '_');

        // Try to determine target level from game interface if not set
        if (options.targetLevel.empty() || options.targetLevel == "Level_01") {
            if (auto *gameInterface = m_Engine->GetGameInterface()) {
                std::string mapName = gameInterface->GetMapName();
                if (!mapName.empty()) {
                    options.targetLevel = mapName;
                }
            }
        }

        // Update description for translation mode
        if (m_IsTranslationMode) {
            if (options.description.find("Translated from") == std::string::npos) {
                options.description = "Translated from legacy record: " + options.description;
            }
        } else if (options.description.empty()) {
            options.description = "Auto-recorded TAS run";
        }

        const char *modeStr = m_IsTranslationMode ? "translation" : "recording";
        m_Engine->GetLogger()->Info("Auto-generating TAS script from %s: %s", modeStr, options.projectName.c_str());

        // Generate the script
        scriptGenerator->GenerateAsync(
            m_Frames, options,
            [this, options, modeStr](bool success) {
                if (success) {
                    m_Engine->GetLogger()->Info("Script auto-generated successfully from %s: %s", modeStr,
                                                options.projectName.c_str());
                    // Refresh projects in project manager
                    if (auto *projectManager = m_Engine->GetProjectManager()) {
                        projectManager->RefreshProjects();
                    }
                } else {
                    m_Engine->GetLogger()->Error("Failed to auto-generate script from %s: %s", modeStr,
                                                 options.projectName.c_str());
                }
            });

        return true;
    } catch (const std::exception &e) {
        const char *modeStr = m_IsTranslationMode ? "translation" : "recording";
        m_Engine->GetLogger()->Error("Exception during script auto-generation from %s: %s", modeStr, e.what());
        return false;
    }
}

void Recorder::Tick(size_t currentTick, const unsigned char *keyboardState) {
    if (!m_IsRecording) {
        return;
    }

    // Check frame limit
    if (m_Frames.size() >= m_MaxFrames) {
        if (!m_WarnedMaxFrames) {
            m_Engine->GetLogger()->Warn("Recording reached maximum frame limit (%zu). Recording will stop.", m_MaxFrames);
            m_WarnedMaxFrames = true;
            Stop();
        }
        return;
    }

    try {
        FrameData frame;
        frame.frameIndex = currentTick;
        frame.deltaTime = m_DeltaTime;
        frame.inputState = CaptureRealInput(keyboardState);

        // Capture physics data
        CapturePhysicsData(frame);

        // Assign any events that were fired since the last tick to this frame
        frame.events = std::move(m_PendingEvents);
        m_PendingEvents.clear();

        m_Frames.emplace_back(std::move(frame));
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Error during recording tick: %s", e.what());
        Stop(); // Stop recording on error to prevent corruption
    }
}

void Recorder::OnGameEvent(size_t currentTick, const std::string &eventName, int eventData) {
    if (!m_IsRecording) {
        return;
    }

    try {
        // Store event in pending list
        m_PendingEvents.emplace_back(currentTick, eventName, eventData);

        m_Engine->GetLogger()->Info("Recorded game event: %s (data: %d) at frame %d",
                                    eventName.c_str(), eventData, currentTick);
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Error recording game event: %s", e.what());
    }
}

bool Recorder::DumpFrameData(const std::string &filePath, bool includePhysics) const {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            m_Engine->GetLogger()->Error("Failed to open file for text dump: %s", filePath.c_str());
            return false;
        }

        // Write header
        file << "# TAS Frame Data\n";
        file << "# Generated: " << GenerateAutoProjectName() << "\n";
        file << "# Total Frames: " << m_Frames.size() << "\n";
        file << "# Delta Time: " << m_DeltaTime << "ms\n";
        if (includePhysics) {
            file << "# Format: Frame | DeltaTime | Input | Position | Velocity | Speed\n";
        } else {
            file << "# Format: Frame | DeltaTime | Input\n";
        }
        file << "\n";

        // Write frame data
        for (const auto &frame : m_Frames) {
            file << frame.frameIndex << " | "
                << std::fixed << std::setprecision(3) << frame.deltaTime << " | "
                << FormatInputStateText(frame.inputState);

            if (includePhysics) {
                file << " | (" << std::fixed << std::setprecision(2)
                    << frame.physics.position.x << ","
                    << frame.physics.position.y << ","
                    << frame.physics.position.z << ")"
                    << " | (" << frame.physics.velocity.x << ","
                    << frame.physics.velocity.y << ","
                    << frame.physics.velocity.z << ")"
                    << " | " << frame.physics.speed;
            }

            file << "\n";

            // Add events if any occurred on this frame
            for (const auto &event : frame.events) {
                file << "\tEVENT: " << event.eventName << " (data: " << event.eventData << ")\n";
            }
        }

        file.close();
        m_Engine->GetLogger()->Info("Frame data text dump saved to: %s", filePath.c_str());
        return true;
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Exception during text dump: %s", e.what());
        return false;
    }
}

bool Recorder::LoadFrameData(const std::string &filePath, bool includePhysics) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            m_Engine->GetLogger()->Error("Failed to open file for loading: %s", filePath.c_str());
            return false;
        }

        // Clear existing data
        ClearFrameData();

        std::string line;
        size_t lineNumber = 0;
        FrameData *currentFrame = nullptr;

        while (std::getline(file, line)) {
            lineNumber++;
            line = TrimString(line);

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // Handle EVENT lines
            if (line.find("\tEVENT: ") == 0) {
                if (!currentFrame) {
                    m_Engine->GetLogger()->Warn("Found EVENT line without frame context at line %zu", lineNumber);
                    continue;
                }

                // Parse EVENT: eventName (data: eventData)
                size_t nameStart = line.find("EVENT: ") + 7;
                size_t dataStart = line.find(" (data: ");
                size_t dataEnd = line.find(')', dataStart);

                if (nameStart == std::string::npos || dataStart == std::string::npos || dataEnd == std::string::npos) {
                    m_Engine->GetLogger()->Warn("Malformed EVENT line at %zu: %s", lineNumber, line.c_str());
                    continue;
                }

                std::string eventName = line.substr(nameStart, dataStart - nameStart);
                std::string dataStr = line.substr(dataStart + 8, dataEnd - dataStart - 8);
                int eventData = std::stoi(dataStr);

                currentFrame->events.emplace_back(currentFrame->frameIndex, eventName, eventData);
                continue;
            }

            // Parse frame data line: Frame | DeltaTime | Input [| Position | Velocity | Speed]
            std::vector<std::string> parts = SplitString(line, '|');
            if (parts.size() < 3) {
                m_Engine->GetLogger()->Warn("Invalid frame data line at %zu: %s", lineNumber, line.c_str());
                continue;
            }

            if (includePhysics && parts.size() < 6) {
                m_Engine->GetLogger()->Warn("Expected physics data but not enough columns at line %zu: %s", lineNumber,
                                            line.c_str());
                continue;
            }

            // Parse frame components
            FrameData frame;
            frame.frameIndex = std::stoul(TrimString(parts[0]));
            frame.deltaTime = std::stof(TrimString(parts[1]));
            frame.inputState = ParseInputStateText(TrimString(parts[2]));

            // Parse physics data if present
            if (includePhysics && parts.size() >= 6) {
                frame.physics.position = ParseVectorText(TrimString(parts[3]));
                frame.physics.velocity = ParseVectorText(TrimString(parts[4]));
                frame.physics.speed = std::stof(TrimString(parts[5]));

                // Calculate derived physics values
                frame.physics.angularSpeed = frame.physics.angularVelocity.Magnitude();
            }

            m_Frames.push_back(frame);
            currentFrame = &m_Frames.back();
        }

        file.close();
        m_Engine->GetLogger()->Info("Loaded %zu frames from: %s", m_Frames.size(), filePath.c_str());
        return true;
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Exception during frame data loading: %s", e.what());
        ClearFrameData();
        return false;
    }
}

bool Recorder::DumpFrameDataBinary(const std::string &filePath) const {
    try {
        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            m_Engine->GetLogger()->Error("Failed to open file for binary dump: %s", filePath.c_str());
            return false;
        }

        // Write header with version and metadata
        const uint32_t version = 1;
        const uint32_t frameCount = static_cast<uint32_t>(m_Frames.size());

        file.write(reinterpret_cast<const char *>(&version), sizeof(version));
        file.write(reinterpret_cast<const char *>(&frameCount), sizeof(frameCount));
        file.write(reinterpret_cast<const char *>(&m_DeltaTime), sizeof(m_DeltaTime));

        // Write frame data
        for (const auto &frame : m_Frames) {
            // Frame basic data
            file.write(reinterpret_cast<const char *>(&frame.frameIndex), sizeof(frame.frameIndex));
            file.write(reinterpret_cast<const char *>(&frame.deltaTime), sizeof(frame.deltaTime));

            // Input state
            file.write(reinterpret_cast<const char *>(&frame.inputState), sizeof(frame.inputState));

            // Physics data
            file.write(reinterpret_cast<const char *>(&frame.physics), sizeof(frame.physics));

            // Events count and data
            uint32_t eventCount = frame.events.size();
            file.write(reinterpret_cast<const char *>(&eventCount), sizeof(eventCount));

            for (const auto &event : frame.events) {
                file.write(reinterpret_cast<const char *>(&event.frame), sizeof(event.frame));
                file.write(reinterpret_cast<const char *>(&event.eventData), sizeof(event.eventData));

                // Write string length and content
                uint32_t nameLength = static_cast<uint32_t>(event.eventName.length());
                file.write(reinterpret_cast<const char *>(&nameLength), sizeof(nameLength));
                file.write(event.eventName.c_str(), nameLength);
            }
        }

        file.close();
        m_Engine->GetLogger()->Info("Frame data binary dump saved to: %s (%zu frames)",
                                    filePath.c_str(), m_Frames.size());
        return true;
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Exception during binary dump: %s", e.what());
        return false;
    }
}

bool Recorder::LoadFrameDataBinary(const std::string &filePath) {
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            m_Engine->GetLogger()->Error("Failed to open file for binary loading: %s", filePath.c_str());
            return false;
        }

        // Clear existing data
        ClearFrameData();

        // Read header
        uint32_t version, frameCount;
        float deltaTime;

        file.read(reinterpret_cast<char *>(&version), sizeof(version));
        file.read(reinterpret_cast<char *>(&frameCount), sizeof(frameCount));
        file.read(reinterpret_cast<char *>(&deltaTime), sizeof(deltaTime));

        if (version != 1) {
            m_Engine->GetLogger()->Error("Unsupported binary format version: %u", version);
            return false;
        }

        m_DeltaTime = deltaTime;
        m_Frames.reserve(frameCount);

        // Read frame data
        for (uint32_t i = 0; i < frameCount; ++i) {
            FrameData frame;

            // Frame basic data
            file.read(reinterpret_cast<char *>(&frame.frameIndex), sizeof(frame.frameIndex));
            file.read(reinterpret_cast<char *>(&frame.deltaTime), sizeof(frame.deltaTime));

            // Input state
            file.read(reinterpret_cast<char *>(&frame.inputState), sizeof(frame.inputState));

            // Physics data
            file.read(reinterpret_cast<char *>(&frame.physics), sizeof(frame.physics));

            // Events
            uint32_t eventCount;
            file.read(reinterpret_cast<char *>(&eventCount), sizeof(eventCount));

            frame.events.reserve(eventCount);
            for (uint32_t j = 0; j < eventCount; ++j) {
                GameEvent event(0, "", 0);

                file.read(reinterpret_cast<char *>(&event.frame), sizeof(event.frame));
                file.read(reinterpret_cast<char *>(&event.eventData), sizeof(event.eventData));

                uint32_t nameLength;
                file.read(reinterpret_cast<char *>(&nameLength), sizeof(nameLength));

                std::string eventName(nameLength, '\0');
                file.read(&eventName[0], nameLength);
                event.eventName = std::move(eventName);

                frame.events.push_back(std::move(event));
            }

            m_Frames.push_back(std::move(frame));
        }

        file.close();
        m_Engine->GetLogger()->Info("Loaded %zu frames from binary file: %s", m_Frames.size(), filePath.c_str());
        return true;
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Exception during binary frame data loading: %s", e.what());
        ClearFrameData();
        return false;
    }
}

std::pair<bool, bool> Recorder::DumpFrameDataBoth(const std::string &basePath, bool includePhysics) const {
    std::string textPath = basePath + ".txt";
    std::string binaryPath = basePath + ".bin";

    bool textSuccess = DumpFrameData(textPath, includePhysics);
    bool binarySuccess = DumpFrameDataBinary(binaryPath);

    m_Engine->GetLogger()->Info("Dual format dump completed - Text: %s, Binary: %s",
                                textSuccess ? "SUCCESS" : "FAILED",
                                binarySuccess ? "SUCCESS" : "FAILED");

    return {textSuccess, binarySuccess};
}

void Recorder::ClearFrameData() {
    m_Frames.clear();
    m_PendingEvents.clear();
}

RawInputState Recorder::CaptureRealInput(const unsigned char *keyboardState) const {
    if (!keyboardState) {
        m_Engine->GetLogger()->Warn("Keyboard state not available.");
        return {};
    }

    RawInputState state;

    state.keyUp = keyboardState[m_KeyUp];
    state.keyDown = keyboardState[m_KeyDown];
    state.keyLeft = keyboardState[m_KeyLeft];
    state.keyRight = keyboardState[m_KeyRight];
    state.keyShift = keyboardState[m_KeyShift];
    state.keySpace = keyboardState[m_KeySpace];
    state.keyQ = keyboardState[CKKEY_Q];
    state.keyEsc = keyboardState[CKKEY_ESCAPE];

    return state;
}

void Recorder::CapturePhysicsData(FrameData &frameData) const {
    try {
        auto *gameInterface = m_Engine->GetGameInterface();
        if (!gameInterface) return;

        // Get ball entity
        auto *ball = gameInterface->GetActiveBall();
        if (!ball) return;

        PhysicsData &physics = frameData.physics;

        // Basic position and velocity
        physics.position = gameInterface->GetPosition(ball);
        physics.velocity = gameInterface->GetVelocity(ball);
        physics.angularVelocity = gameInterface->GetAngularVelocity(ball);

        // Derived values
        physics.speed = physics.velocity.Magnitude();
        physics.angularSpeed = physics.angularVelocity.Magnitude();
    } catch (const std::exception &) {
        // Don't log physics capture errors as they're non-critical
        frameData.physics = PhysicsData{}; // Reset to defaults
    }
}

void Recorder::NotifyStatusChange(bool isRecording) {
    if (m_StatusCallback) {
        try {
            m_StatusCallback(isRecording);
        } catch (const std::exception &e) {
            m_Engine->GetLogger()->Error("Error in recording status callback: %s", e.what());
        }
    }
}

std::string Recorder::FormatInputStateText(const RawInputState &rawInput) {
    std::string result;

    auto addKey = [&](const std::string &name, uint8_t state) {
        if (state != KS_IDLE) result.append(name);
        if (state & KS_PRESSED) result.append("+");
        if (state & KS_RELEASED) result.append("-");
    };

    addKey("U", rawInput.keyUp);
    addKey("D", rawInput.keyDown);
    addKey("L", rawInput.keyLeft);
    addKey("R", rawInput.keyRight);
    addKey("S", rawInput.keyShift);
    addKey("SP", rawInput.keySpace);
    addKey("Q", rawInput.keyQ);
    addKey("ESC", rawInput.keyEsc);

    return result.empty() ? "IDLE" : result;
}

RawInputState Recorder::ParseInputStateText(const std::string &inputText) {
    RawInputState state;

    if (inputText == "IDLE") {
        return state; // All fields are already KS_IDLE (0)
    }

    // Parse input like "U+D+-L+R+" where keys can have both + and - flags
    size_t pos = 0;
    while (pos < inputText.length()) {
        // Find the start of the next key (uppercase letter or "SP"/"ESC")
        size_t keyStart = pos;

        // Find key name end (look for + or - or end of string)
        size_t keyEnd = pos;
        while (keyEnd < inputText.length() && inputText[keyEnd] != '+' && inputText[keyEnd] != '-') {
            keyEnd++;
        }

        if (keyEnd == keyStart) {
            pos++;
            continue; // Skip invalid characters
        }

        std::string keyName = inputText.substr(keyStart, keyEnd - keyStart);

        // Parse flags following the key name
        uint8_t keyState = KS_IDLE;
        pos = keyEnd;
        while (pos < inputText.length() && (inputText[pos] == '+' || inputText[pos] == '-')) {
            if (inputText[pos] == '+') {
                keyState |= KS_PRESSED;
            } else if (inputText[pos] == '-') {
                keyState |= KS_RELEASED;
            }
            pos++;
        }

        // Map key names to state fields
        if (keyName == "U") state.keyUp = keyState;
        else if (keyName == "D") state.keyDown = keyState;
        else if (keyName == "L") state.keyLeft = keyState;
        else if (keyName == "R") state.keyRight = keyState;
        else if (keyName == "S") state.keyShift = keyState;
        else if (keyName == "SP") state.keySpace = keyState;
        else if (keyName == "Q") state.keyQ = keyState;
        else if (keyName == "ESC") state.keyEsc = keyState;
    }

    return state;
}

VxVector Recorder::ParseVectorText(const std::string &posText) {
    // Parse "(x,y,z)" format
    if (posText.length() < 5 || posText[0] != '(' || posText.back() != ')') {
        return VxVector(0, 0, 0);
    }

    std::string content = posText.substr(1, posText.length() - 2);
    std::vector<std::string> parts = SplitString(content, ',');

    if (parts.size() != 3) {
        return VxVector(0, 0, 0);
    }

    return VxVector(
        std::stof(TrimString(parts[0])),
        std::stof(TrimString(parts[1])),
        std::stof(TrimString(parts[2]))
    );
}

std::string Recorder::TrimString(const std::string &str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";

    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::vector<std::string> Recorder::SplitString(const std::string &str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }

    return result;
}
