#include "EngineBootstrap.h"

#include "TASConstants.h"
#include "ContextLifecycleCoordinator.h"
#include "EventBus.h"
#include "EventManager.h"
#include "GameInterface.h"
#include "HookManager.h"
#include "InputSystem.h"
#include "Logger.h"
#include "LuaTypedEventBridge.h"
#include "PlaybackService.h"
#include "ProjectManager.h"
#include "RecordPlayer.h"
#include "Recorder.h"
#include "RecordingService.h"
#include "RuntimeEventRouter.h"
#include "DeterminismVerifier.h"
#include "SavestateManager.h"
#include "ScriptContextManager.h"
#include "ScriptGenerator.h"
#include "ServiceContainer.h"
#include "StartupProjectManager.h"
#include "TASEngine.h"
#include "TASProject.h"
#include "TASStateHandlers.h"
#include "TASStateMachine.h"
#include "TranslationService.h"
#include "ValidationService.h"

#ifdef ENABLE_REPL
#include "LuaREPLServer.h"
#endif

bool EngineBootstrap::InitializeCoreSubsystems(TASEngine &engine) {
    try {
        auto &c = engine.m_ServiceContainer;

        // Register externally-owned pointers (BallanceTAS lifetime)
        c.RegisterSingletonPtr<GameInterface>(engine.m_GameInterface);
        c.RegisterSingletonPtr<EventBus>(engine.m_EventBus);
        c.RegisterSingletonPtr<HookManager>(engine.m_HookManager);

        // Register GameInterface against narrow ISP interfaces
        c.RegisterSingletonPtr<IGameControl>(engine.m_GameInterface);
        c.RegisterSingletonPtr<IGameQuery>(engine.m_GameInterface);
        c.RegisterSingletonPtr<IInputAccess>(engine.m_GameInterface);
        c.RegisterSingletonPtr<IPhysicsProvider>(engine.m_GameInterface);
        c.RegisterSingletonPtr<IObjectProvider>(engine.m_GameInterface);
        c.RegisterSingletonPtr<IGameStateProvider>(engine.m_GameInterface);
        c.RegisterSingletonPtr<ITimeProvider>(engine.m_GameInterface);

        // Register OperationRequestStore (non-owning ptr to TASEngine's member)
        c.RegisterSingletonPtr<OperationRequestStore>(&engine.m_Requests);

        // Core subsystems — container takes ownership
        c.RegisterSingletonInstance<InputSystem>(std::make_unique<InputSystem>());
        c.RegisterSingletonInstance<EventManager>(std::make_unique<EventManager>());
        c.RegisterSingletonInstance<Recorder>(std::make_unique<Recorder>(&engine));
        c.RegisterSingletonInstance<ScriptGenerator>(std::make_unique<ScriptGenerator>(&engine));
        c.RegisterSingletonInstance<ScriptContextManager>(std::make_unique<ScriptContextManager>(&engine));
        c.RegisterSingletonInstance<RecordPlayer>(std::make_unique<RecordPlayer>(&engine));
        c.RegisterSingletonInstance<StartupProjectManager>(std::make_unique<StartupProjectManager>(&engine));
        c.RegisterSingletonInstance<ProjectManager>(std::make_unique<ProjectManager>(&engine));

        // Services with dependencies
        c.RegisterSingletonInstance<RecordingService>(
            std::make_unique<RecordingService>(&engine.m_ServiceProvider));
        c.RegisterSingletonInstance<TranslationService>(
            std::make_unique<TranslationService>(
                *c.Resolve<Recorder>(),
                *c.Resolve<RecordPlayer>(),
                *engine.m_GameInterface,
                *engine.m_GameInterface,
                *engine.m_GameInterface,
                *c.Resolve<ScriptContextManager>(),
                *c.Resolve<InputSystem>(),
                *engine.m_HookManager,
                *engine.m_EventBus,
                engine.GetPath()));
        c.RegisterSingletonInstance<ValidationService>(
            std::make_unique<ValidationService>(
                *c.Resolve<Recorder>(),
                *engine.m_EventBus));
        c.RegisterSingletonInstance<SavestateManager>(
            std::make_unique<SavestateManager>(engine.m_ServiceProvider));
        c.RegisterSingletonInstance<DeterminismVerifier>(
            std::make_unique<DeterminismVerifier>(engine.m_ServiceProvider));

        // State machine — uses EventBus directly
        c.RegisterSingletonInstance<TASStateMachine>(
            std::make_unique<TASStateMachine>(engine.m_EventBus));

        auto *sm = c.Resolve<TASStateMachine>();
        sm->RegisterHandler(TASStateMachine::State::Idle,
                            std::make_unique<IdleHandler>(&engine));
        sm->RegisterHandler(TASStateMachine::State::PendingRecord,
                            std::make_unique<PendingRecordHandler>(&engine));
        sm->RegisterHandler(TASStateMachine::State::Recording,
                            std::make_unique<RecordingHandler>(&engine));
        sm->RegisterHandler(TASStateMachine::State::PendingScriptPlayback,
                            std::make_unique<PendingScriptPlaybackHandler>(&engine));
        sm->RegisterHandler(TASStateMachine::State::PendingRecordPlayback,
                            std::make_unique<PendingRecordPlaybackHandler>(&engine));
        sm->RegisterHandler(TASStateMachine::State::PlayingScript,
                            std::make_unique<PlayingScriptHandler>(&engine));
        sm->RegisterHandler(TASStateMachine::State::PlayingRecord,
                            std::make_unique<PlayingRecordHandler>(&engine));
        sm->RegisterHandler(TASStateMachine::State::PendingTranslation,
                            std::make_unique<PendingTranslationHandler>(&engine));
        sm->RegisterHandler(TASStateMachine::State::Translating,
                            std::make_unique<TranslatingHandler>(&engine));
        sm->RegisterHandler(TASStateMachine::State::Paused,
                            std::make_unique<PausedHandler>(&engine));
        sm->RegisterHandler(TASStateMachine::State::ShuttingDown,
                            std::make_unique<ShuttingDownHandler>(&engine));

        Log::Info("Core runtime subsystems initialized.");
        return true;
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize core subsystems: %s", e.what());
        return false;
    }
}

bool EngineBootstrap::InitializeHighLevelSubsystems(TASEngine &engine) {
    try {
        auto &c = engine.m_ServiceContainer;

        auto *scriptContextManager = c.Resolve<ScriptContextManager>();
        if (scriptContextManager && !scriptContextManager->Initialize()) {
            Log::Error("Failed to initialize ScriptContextManager.");
            return false;
        }

        LuaREPLServer *replPtr = nullptr;
#ifdef ENABLE_REPL
        c.RegisterSingletonInstance<LuaREPLServer>(
            std::make_unique<LuaREPLServer>(&engine));
        auto *replServer = c.Resolve<LuaREPLServer>();
        if (replServer->Initialize(TASConstants::DefaultREPLPort, "")) {
            if (replServer->Start()) {
                Log::Info("REPL server started on port %u", TASConstants::DefaultREPLPort);
                replPtr = replServer;
            } else {
                Log::Warn("Failed to start REPL server.");
            }
        } else {
            Log::Warn("Failed to initialize REPL server.");
        }
#endif
        c.RegisterSingletonInstance<PlaybackService>(
            std::make_unique<PlaybackService>(&engine.m_ServiceProvider));
        if (auto *playbackService = c.Resolve<PlaybackService>()) {
            playbackService->SetCompletionCallback([&engine]() {
                engine.HandlePlaybackCompleted();
            });
        }

        auto *startupProjectManager = c.Resolve<StartupProjectManager>();
        if (startupProjectManager && !startupProjectManager->Initialize()) {
            Log::Error("Failed to initialize StartupProjectManager.");
            return false;
        }

        c.RegisterSingletonInstance<ContextLifecycleCoordinator>(
            std::make_unique<ContextLifecycleCoordinator>(
                *scriptContextManager,
                *engine.m_GameInterface));
        c.RegisterSingletonInstance<LuaTypedEventBridge>(
            std::make_unique<LuaTypedEventBridge>(
                *engine.m_EventBus,
                *scriptContextManager,
                c.Resolve<Recorder>(),
                *c.Resolve<TASStateMachine>(),
                [&engine]() { return engine.GetCurrentTick(); }));
        c.RegisterSingletonInstance<RuntimeEventRouter>(
            std::make_unique<RuntimeEventRouter>(
                *engine.m_EventBus,
                *c.Resolve<TASStateMachine>(),
                *c.Resolve<ContextLifecycleCoordinator>(),
                c.Resolve<PlaybackService>(),
                c.Resolve<TranslationService>(),
                c.Resolve<ValidationService>(),
                engine.m_Requests,
                [&engine]() { return engine.IsValidationEnabled(); },
                [&engine](TASProject *project) { return engine.BuildValidationOutputPath(project); }));

        auto *bridge = c.Resolve<LuaTypedEventBridge>();
        if (bridge) {
            bridge->Initialize();
        }
        auto *router = c.Resolve<RuntimeEventRouter>();
        if (router) {
            router->Initialize();
        }

        Log::Info("TASEngine and all subsystems initialized.");
        return true;
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize higher-level subsystems: %s", e.what());
        return false;
    }
}
