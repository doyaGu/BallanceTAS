/**
 * @file EngineBootstrap.cpp
 * @brief Composition root — all subsystem construction and wiring lives here.
 *
 * This is the ONE file that is allowed to know about every concrete subsystem
 * type.  TASEngine.cpp only needs EngineBootstrap.h and delegates here.
 */

#include "EngineBootstrap.h"

#include "TASEngine.h"
#include "Logger.h"
#include "GameInterface.h"
#include "InputSystem.h"
#include "EventManager.h"
#include "Recorder.h"
#include "ScriptGenerator.h"
#include "ScriptContextManager.h"
#include "ScriptContext.h"
#include "RecordPlayer.h"
#include "StartupProjectManager.h"
#include "ProjectManager.h"
#include "TASStateMachine.h"
#include "TASStateHandlers.h"
#include "SavestateManager.h"
#include "ServiceContainer.h"
#include "UIManager.h"

// Services
#include "RecordingService.h"
#include "PlaybackService.h"
#include "TranslationService.h"
#include "ValidationService.h"
#include "EventBus.h"
#include "HookManager.h"

#ifdef ENABLE_REPL
#include "LuaREPLServer.h"
#endif

// ============================================================================
// Phase 1 — Core subsystems (ServiceContainer, state machine, controllers)
// ============================================================================

bool EngineBootstrap::InitializeCoreSubsystems(TASEngine &engine) {
    // 0. Create ServiceContainer
    try {
        engine.m_ServiceContainer = std::make_unique<ServiceContainer>();
        Log::Info("ServiceContainer initialized.");

        // Register external dependencies (TASEngine retains ownership)
        engine.m_ServiceContainer->RegisterSingletonPtr(engine.m_GameInterface);
        engine.m_ServiceContainer->RegisterSingletonPtr(&engine);
        engine.m_ServiceContainer->RegisterSingletonPtr(engine.m_EventBus);
        engine.m_ServiceContainer->RegisterSingletonPtr(engine.m_HookManager);
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize ServiceContainer: %s", e.what());
        return false;
    }

    // 1. Create core subsystems and transfer ownership to ServiceContainer
    try {
        auto inputSystem = std::make_unique<InputSystem>();
        auto eventManager = std::make_unique<EventManager>();
        auto recorder = std::make_unique<Recorder>(&engine);
        auto scriptGenerator = std::make_unique<ScriptGenerator>(&engine);
        auto scriptContextManager = std::make_unique<ScriptContextManager>(&engine);
        auto recordPlayer = std::make_unique<RecordPlayer>(&engine);
        auto startupProjectManager = std::make_unique<StartupProjectManager>(&engine);

        engine.m_ServiceContainer->RegisterSingletonInstance(std::move(inputSystem));
        engine.m_ServiceContainer->RegisterSingletonInstance(std::move(eventManager));
        engine.m_ServiceContainer->RegisterSingletonInstance(std::move(recorder));
        engine.m_ServiceContainer->RegisterSingletonInstance(std::move(scriptGenerator));
        engine.m_ServiceContainer->RegisterSingletonInstance(std::move(scriptContextManager));
        engine.m_ServiceContainer->RegisterSingletonInstance(std::move(recordPlayer));
        engine.m_ServiceContainer->RegisterSingletonInstance(std::move(startupProjectManager));

        // State machine
        auto stateMachine = std::make_unique<TASStateMachine>(&engine);

        stateMachine->RegisterHandler(TASStateMachine::State::Idle,
                                      std::make_unique<IdleHandler>(&engine));
        stateMachine->RegisterHandler(TASStateMachine::State::PendingRecord,
                                      std::make_unique<PendingRecordHandler>(&engine));
        stateMachine->RegisterHandler(TASStateMachine::State::Recording,
                                      std::make_unique<RecordingHandler>(&engine));
        stateMachine->RegisterHandler(TASStateMachine::State::PendingScriptPlayback,
                                      std::make_unique<PendingScriptPlaybackHandler>(&engine));
        stateMachine->RegisterHandler(TASStateMachine::State::PendingRecordPlayback,
                                      std::make_unique<PendingRecordPlaybackHandler>(&engine));
        stateMachine->RegisterHandler(TASStateMachine::State::PlayingScript,
                                      std::make_unique<PlayingScriptHandler>(&engine));
        stateMachine->RegisterHandler(TASStateMachine::State::PlayingRecord,
                                      std::make_unique<PlayingRecordHandler>(&engine));
        stateMachine->RegisterHandler(TASStateMachine::State::PendingTranslation,
                                      std::make_unique<PendingTranslationHandler>(&engine));
        stateMachine->RegisterHandler(TASStateMachine::State::Translating,
                                      std::make_unique<TranslatingHandler>(&engine));
        stateMachine->RegisterHandler(TASStateMachine::State::Paused,
                                      std::make_unique<PausedHandler>(&engine));
        stateMachine->RegisterHandler(TASStateMachine::State::ShuttingDown,
                                      std::make_unique<ShuttingDownHandler>(&engine));

        Log::Info("State machine initialized with all handlers registered.");
        engine.m_ServiceContainer->RegisterSingletonInstance(std::move(stateMachine));

        // Services
        auto provider = engine.GetServiceProvider();

        auto recordingService = std::make_unique<RecordingService>(provider);
        auto playbackService = std::make_unique<PlaybackService>(provider);
        auto translationService = std::make_unique<TranslationService>(provider);
        auto validationService = std::make_unique<ValidationService>(provider);

        // Wire services to shared infrastructure
        recordingService->SetEventBus(engine.m_EventBus);
        recordingService->SetHookManager(engine.m_HookManager);
        playbackService->SetEventBus(engine.m_EventBus);
        playbackService->SetHookManager(engine.m_HookManager);
        translationService->SetEventBus(engine.m_EventBus);
        translationService->SetHookManager(engine.m_HookManager);
        validationService->SetEventBus(engine.m_EventBus);

        engine.m_ServiceContainer->RegisterSingletonInstance(std::move(recordingService));
        engine.m_ServiceContainer->RegisterSingletonInstance(std::move(playbackService));
        engine.m_ServiceContainer->RegisterSingletonInstance(std::move(translationService));
        engine.m_ServiceContainer->RegisterSingletonInstance(std::move(validationService));

        Log::Info("Services initialized.");

        // SavestateManager
        auto savestateManager = std::make_unique<SavestateManager>(provider);
        engine.m_ServiceContainer->RegisterSingletonInstance(std::move(savestateManager));

        Log::Info("SavestateManager initialized.");
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize core subsystems: %s", e.what());
        return false;
    }

    return true;
}

// ============================================================================
// Phase 2 — Higher-level subsystems (scripting, projects, callbacks)
// ============================================================================

bool EngineBootstrap::InitializeHighLevelSubsystems(TASEngine &engine) {
    // 1. Initialize execution subsystems
    try {
        auto *scriptCtxMgr = engine.m_ServiceContainer->Resolve<ScriptContextManager>();
        if (scriptCtxMgr && !scriptCtxMgr->Initialize()) {
            Log::Error("Failed to initialize ScriptContextManager.");
            return false;
        }

#ifdef ENABLE_REPL
        auto replServer = std::make_unique<LuaREPLServer>(&engine);
        if (replServer->Initialize(7878, "")) {
            if (replServer->Start()) {
                Log::Info("REPL server started on port 7878");
                engine.m_ServiceContainer->RegisterSingletonInstance(std::move(replServer));
            } else {
                Log::Warn("Failed to start REPL server, not registering in container");
            }
        } else {
            Log::Warn("Failed to initialize REPL server, not registering in container");
        }
#endif
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize execution subsystems: %s", e.what());
        return false;
    }

    // 2. Initialize Project Manager
    try {
        auto projectManager = std::make_unique<ProjectManager>(&engine);
        engine.m_ServiceContainer->RegisterSingletonInstance(std::move(projectManager));
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize project manager: %s", e.what());
        return false;
    }

    // 3. Initialize StartupProjectManager
    try {
        auto *startupMgr = engine.m_ServiceContainer->Resolve<StartupProjectManager>();
        if (startupMgr && !startupMgr->Initialize()) {
            Log::Error("Failed to initialize StartupProjectManager.");
            return false;
        }
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize startup project manager: %s", e.what());
        return false;
    }

    Log::Info("TASEngine and all subsystems initialized.");
    Log::Info("ServiceContainer holds %zu registered services.",
              engine.m_ServiceContainer->GetServiceCount());
    return true;
}
