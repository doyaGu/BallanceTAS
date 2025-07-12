#include "Recorder.h"

#include <algorithm>
#include <chrono>
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

        // Ensure description is not empty
        if (options.description.empty()) {
            options.description = "Auto-recorded TAS run";
        }

        m_Engine->GetLogger()->Info("Auto-generating TAS script: %s", options.projectName.c_str());

        // Generate the script
        scriptGenerator->GenerateAsync(
            m_Frames, options,
            [this, options](bool success) {
                if (success) {
                    m_Engine->GetLogger()->Info("Script auto-generated successfully: %s", options.projectName.c_str());
                    // Refresh projects in project manager
                    if (auto *projectManager = m_Engine->GetProjectManager()) {
                        projectManager->RefreshProjects();
                    }
                } else {
                    m_Engine->GetLogger()->Error(
                        "Failed to auto-generate script: %s", options.projectName.c_str());
                }
            });

        return true;
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Exception during script auto-generation: %s", e.what());
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
