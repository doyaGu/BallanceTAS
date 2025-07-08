#include "Recorder.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "TASEngine.h"
#include "BallanceTAS.h"
#include "GameInterface.h"
#include "ProjectManager.h"
#include "ScriptGenerator.h"

Recorder::Recorder(TASEngine *engine)
    : m_Engine(engine), m_Mod(engine->GetMod()), m_BML(m_Mod->GetBML()) {
    if (!m_Engine || !m_Mod || !m_BML) {
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
    m_GenerationOptions->optimizeShortWaits = true;
    m_GenerationOptions->addFrameComments = true;
    m_GenerationOptions->addPhysicsComments = false;
    m_GenerationOptions->groupSimilarActions = true;
}

void Recorder::SetGenerationOptions(const GenerationOptions &options) {
    *m_GenerationOptions = options;
}

void Recorder::Start() {
    if (m_IsRecording) {
        m_Mod->GetLogger()->Warn("Recorder is already recording. Stopping previous session.");
        Stop();
    }

    // Clear previous data
    m_Frames.clear();
    m_PendingEvents.clear();
    m_CurrentTick = 0;
    m_WarnedMaxFrames = false;

    // Get starting frame from game interface
    if (auto *gameInterface = m_Engine->GetGameInterface()) {
        m_CurrentTick = gameInterface->GetCurrentTick();
    }

    m_IsRecording = true;
    NotifyStatusChange(true);

    m_Mod->GetLogger()->Info("Recording started at frame %d", m_CurrentTick);
}

std::vector<RawFrameData> Recorder::Stop() {
    if (!m_IsRecording) {
        m_Mod->GetLogger()->Warn("Recorder is not currently recording.");
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

    m_Mod->GetLogger()->Info("Recording stopped. Captured %zu frames over %d ticks.",
                             m_Frames.size(), m_CurrentTick);

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
        m_Mod->GetLogger()->Warn("No frames recorded, cannot generate script.");
        return false;
    }

    try {
        // Get script generator from engine
        auto *scriptGenerator = m_Engine->GetScriptGenerator();
        if (!scriptGenerator) {
            m_Mod->GetLogger()->Error("ScriptGenerator not available.");
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

        m_Mod->GetLogger()->Info("Auto-generating TAS script: %s", options.projectName.c_str());

        // Generate the script
        bool success = scriptGenerator->Generate(m_Frames, options);
        if (!success) {
            m_Mod->GetLogger()->Error("Failed to auto-generate script from recording.");
            return false;
        }

        m_Mod->GetLogger()->Info("Recording auto-generated successfully: %s", options.projectName.c_str());

        // Refresh projects in project manager
        if (auto *projectManager = m_Engine->GetProjectManager()) {
            projectManager->RefreshProjects();
        }

        return true;

    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Exception during script auto-generation: %s", e.what());
        return false;
    }
}

void Recorder::Tick() {
    if (!m_IsRecording) {
        return;
    }

    // Check frame limit
    if (m_Frames.size() >= m_MaxFrames) {
        if (!m_WarnedMaxFrames) {
            m_Mod->GetLogger()->Warn("Recording reached maximum frame limit (%zu). Recording will stop.", m_MaxFrames);
            m_WarnedMaxFrames = true;
            Stop();
        }
        return;
    }

    try {
        RawFrameData frame;
        frame.frameIndex = m_CurrentTick;
        frame.inputState = CaptureRealInput();

        // Capture physics data for validation
        CapturePhysicsData(frame);

        // Assign any events that were fired since the last tick to this frame
        frame.events = std::move(m_PendingEvents);
        m_PendingEvents.clear();

        m_Frames.emplace_back(std::move(frame));
        m_CurrentTick++;
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Error during recording tick: %s", e.what());
        Stop(); // Stop recording on error to prevent corruption
    }
}

void Recorder::OnGameEvent(const std::string &eventName, int eventData) {
    if (!m_IsRecording) {
        return;
    }

    try {
        // Store event in pending list - it will be associated with the next frame
        m_PendingEvents.emplace_back(eventName, eventData);

        m_Mod->GetLogger()->Info("Recorded game event: %s (data: %d) at frame %d",
                                 eventName.c_str(), eventData, m_CurrentTick);
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Error recording game event: %s", e.what());
    }
}

RawInputState Recorder::CaptureRealInput() const {
    auto *inputManager = m_BML->GetInputManager();
    if (!inputManager) {
        return {};
    }

    // CRITICAL: Use 'oIsKeyDown' functions to get the original,
    // un-synthesized keyboard state representing actual player input
    RawInputState state;

    try {
        state.keyUp = inputManager->oIsKeyDown(CKKEY_UP);
        state.keyDown = inputManager->oIsKeyDown(CKKEY_DOWN);
        state.keyLeft = inputManager->oIsKeyDown(CKKEY_LEFT);
        state.keyRight = inputManager->oIsKeyDown(CKKEY_RIGHT);
        state.keyShift = inputManager->oIsKeyDown(CKKEY_LSHIFT) || inputManager->oIsKeyDown(CKKEY_RSHIFT);
        state.keySpace = inputManager->oIsKeyDown(CKKEY_SPACE);
        state.keyQ = inputManager->oIsKeyDown(CKKEY_Q);
        state.keyEsc = inputManager->oIsKeyDown(CKKEY_ESCAPE);
    } catch (const std::exception &e) {
        m_Mod->GetLogger()->Error("Error capturing input state: %s", e.what());
        return {}; // Return empty state on error
    }

    return state;
}

void Recorder::CapturePhysicsData(RawFrameData &frameData) const {
    try {
        auto *gameInterface = m_Engine->GetGameInterface();
        if (!gameInterface) return;

        // Get ball velocity for validation/debugging
        auto *ball = gameInterface->GetBall();
        if (ball) {
            VxVector velocity = gameInterface->GetVelocity(ball);
            frameData.ballSpeed = velocity.Magnitude();
            frameData.isOnGround = gameInterface->IsOnGround();
        }
    } catch (const std::exception &) {
        // Don't log physics capture errors as they're non-critical
        frameData.ballSpeed = 0.0f;
        frameData.isOnGround = false;
    }
}

void Recorder::NotifyStatusChange(bool isRecording) {
    if (m_StatusCallback) {
        try {
            m_StatusCallback(isRecording);
        } catch (const std::exception &e) {
            m_Mod->GetLogger()->Error("Error in recording status callback: %s", e.what());
        }
    }
}