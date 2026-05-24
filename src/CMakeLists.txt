# =============================================================================
# BallanceTAS – Modular Build
#
# The sources are split into focused STATIC library targets. Each module has
# explicit dependency declarations so that:
#   1. Changing a file only rebuilds the modules that actually depend on it.
#   2. The allowed #include directions are enforced via target_link_libraries.
#   3. New subsystems can be added to one module without touching the others.
#
# All static libraries are linked into the final BallanceTAS shared library
# (.bmodp DLL loaded by BML).
# =============================================================================

# Helper: every module can #include files in src/
set(TAS_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# ── Module: core ─────────────────────────────────────────────────────────────
#    Pure C++ infrastructure – no Virtools and no Lua dependency.
# ─────────────────────────────────────────────────────────────────────────────
add_library(tas_core STATIC
        Result.h
        PlaybackTypes.h
        EventBus.h
        GameEvents.h
        RuntimeSession.h            RuntimeSession.cpp
        TASConstants.h
        ILogSink.h
        ServiceContainer.h          ServiceContainer.cpp
        Logger.h                    Logger.cpp
        ThreadOwnershipValidator.h
        LockFreeMPSCQueue.h
        ResourceManager.h           ResourceManager.cpp
        DeterminismTrace.h          DeterminismTrace.cpp
)
target_include_directories(tas_core
        PUBLIC  ${TAS_SRC_DIR}
)
# tas_core is now pure C++ — no BML / VirtoolsSDK dependency.
set_target_properties(tas_core PROPERTIES FOLDER "TAS/Modules")

# ── Module: lua_runtime ──────────────────────────────────────────────────────
#    Project-owned Lua C API abstraction.
add_library(tas_lua_runtime STATIC
        LuaRuntime/LuaState.h          LuaRuntime/LuaState.cpp
        LuaRuntime/LuaStackGuard.h
        LuaRuntime/LuaRef.h            LuaRuntime/LuaRef.cpp
        LuaRuntime/LuaFunction.h       LuaRuntime/LuaFunction.cpp
        LuaRuntime/LuaThread.h         LuaRuntime/LuaThread.cpp
        LuaRuntime/LuaProtectedCall.h  LuaRuntime/LuaProtectedCall.cpp
        LuaRuntime/LuaValue.h          LuaRuntime/LuaValue.cpp
        LuaRuntime/LuaYieldState.h
        LuaRuntime/LuaUserdata.h
        LuaRuntime/LuaBinder.h
)
target_link_libraries(tas_lua_runtime
        PUBLIC tas_core lua::lua
)
target_include_directories(tas_lua_runtime
        PUBLIC ${TAS_SRC_DIR}
)
set_target_properties(tas_lua_runtime PROPERTIES FOLDER "TAS/Modules")

# ── Module: game ─────────────────────────────────────────────────────────────
#    Everything below requires VirtoolsSDK & BML → only built with BUILD_MOD.
# ─────────────────────────────────────────────────────────────────────────────
if(BUILD_MOD)
add_library(tas_game STATIC
        GameInterface.h             GameInterface.cpp
        IGameControl.h
        IGameQuery.h
        IInputAccess.h
        IPhysicsProvider.h
        IObjectProvider.h
        IGameStateProvider.h
        ITimeProvider.h
        physics_RT.h                physics_RT.cpp
        PhysicsWorldSnapshot.h      PhysicsWorldSnapshot.cpp
        InputSystem.h               InputSystem.cpp
        TASHook.h
        Hook.h
)
target_link_libraries(tas_game
        PUBLIC  tas_core BML CK2 VxMath minhook
)
set_target_properties(tas_game PROPERTIES FOLDER "TAS/Modules")

# ── Module: scripting ────────────────────────────────────────────────────────
#    Lua execution runtime – contexts, scheduler, inter-context comms.
# ─────────────────────────────────────────────────────────────────────────────
add_library(tas_scripting STATIC
        ScriptContextTypes.h
        ScriptContext.h             ScriptContext.cpp
        ScriptContextManager.h      ScriptContextManager.cpp
        LuaScheduler.h              LuaScheduler.cpp
        EventManager.h              EventManager.cpp
        SharedDataManager.h         SharedDataManager.cpp
        MessageBus.h                MessageBus.cpp
        SharedBuffer.h
        AsyncTask.h                 AsyncTask.cpp
)
target_link_libraries(tas_scripting
        PUBLIC  tas_core tas_lua_runtime lua::lua yyjson
        PRIVATE tas_game
)
set_target_properties(tas_scripting PROPERTIES FOLDER "TAS/Modules")

# ── Module: features ─────────────────────────────────────────────────────────
#    TAS operation layer – engine coordinator, services, recording, playback,
#    savestate, project management.
# ─────────────────────────────────────────────────────────────────────────────
add_library(tas_features STATIC
        TASEngine.h                 TASEngine.cpp
        EngineBootstrap.h           EngineBootstrap.cpp
        OperationRequestStore.h
        RuntimeEventRouter.h        RuntimeEventRouter.cpp
        ContextLifecycleCoordinator.h ContextLifecycleCoordinator.cpp
        LuaTypedEventBridge.h       LuaTypedEventBridge.cpp
        HookManager.h               HookManager.cpp
        ConfigService.h             ConfigService.cpp
        TASProject.h                TASProject.cpp
        ProjectManager.h            ProjectManager.cpp
        Recorder.h                  Recorder.cpp
        RecordPlayer.h              RecordPlayer.cpp
        RecordFileIO.h              RecordFileIO.cpp
        RecordingService.h          RecordingService.cpp
        PlaybackService.h           PlaybackService.cpp
        TranslationService.h        TranslationService.cpp
        ScriptInputTransition.h     ScriptInputTransition.cpp
        ValidationService.h         ValidationService.cpp
        ScriptGenerator.h           ScriptGenerator.cpp
        ScriptGenerationCore.h      ScriptGenerationCore.cpp
        StartupProjectManager.h     StartupProjectManager.cpp
        SavestateManager.h          SavestateManager.cpp
        DeterminismVerifier.h       DeterminismVerifier.cpp
        LuaREPLServer.h             LuaREPLServer.cpp
)
target_link_libraries(tas_features
        PUBLIC  tas_core tas_game tas_scripting
        PRIVATE yyjson zip fmt::fmt
)
set_target_properties(tas_features PROPERTIES FOLDER "TAS/Modules")

# ── Module: lua_bindings ─────────────────────────────────────────────────────
#    All LuaApi/* binding translation units – isolated so that core/game
#    changes do not trigger a rebuild of ~30 binding files and vice versa.
# ─────────────────────────────────────────────────────────────────────────────
add_library(tas_lua_bindings STATIC
        LuaApi/LuaApi.h
        LuaApi/LuaApi.cpp
        LuaApi/LuaApi_Core.cpp
        LuaApi/LuaApi_CKEnums.cpp
        LuaApi/LuaApi_CKObject.cpp
        LuaApi/LuaApi_CKSceneObject.cpp
        LuaApi/LuaApi_CKBeObject.cpp
        LuaApi/LuaApi_CKRenderObject.cpp
        LuaApi/LuaApi_CK3dEntity.cpp
        LuaApi/LuaApi_CKCamera.cpp
        LuaApi/LuaApi_PhysicsObject.cpp
        LuaApi/LuaApi_Determinism.cpp
        LuaApi/LuaApi_Savestate.cpp
        LuaApi/LuaApi_GC.cpp
        LuaApi/LuaApi_Debug.cpp
        LuaApi/LuaApi_Result.cpp
        LuaApi/LuaApi_Project.cpp
        LuaApi/LuaApi_Record.cpp
        LuaApi/LuaApi_Level.cpp
        LuaApi/LuaApi_Global.cpp
        LuaApi/LuaApi_Menu.cpp
        LuaApi/LuaApi_SharedBuffer.cpp
        LuaApi/LuaApi_Input.cpp
        LuaApi/LuaApi_VxVector.cpp
        LuaApi/LuaApi_VxColor.cpp
        LuaApi/LuaApi_VxRect.cpp
        LuaApi/LuaApi_VxQuaternion.cpp
        LuaApi/LuaApi_VxMatrix.cpp
        LuaApi/LuaApi_VxIntersectionDesc.cpp
        LuaApi/LuaApi_World.cpp
        LuaApi/LuaApi_Concurrency.cpp
        LuaApi/LuaApi_Async.cpp
        LuaApi/LuaApi_Context.cpp
)
target_link_libraries(tas_lua_bindings
        PRIVATE tas_core tas_game tas_scripting tas_features fmt::fmt
)
set_target_properties(tas_lua_bindings PROPERTIES FOLDER "TAS/Modules")

# ── Module: ui ───────────────────────────────────────────────────────────────
#    User interface layer (TAS menu, in-game OSD).
# ─────────────────────────────────────────────────────────────────────────────
add_library(tas_ui STATIC
        UIManager.h                 UIManager.cpp
        TASMenu.h                   TASMenu.cpp
        TASMenuPresentation.h       TASMenuPresentation.cpp
        InGameOSD.h                 InGameOSD.cpp
)
target_link_libraries(tas_ui
        PUBLIC  tas_core
        PRIVATE tas_features BML
)
set_target_properties(tas_ui PROPERTIES FOLDER "TAS/Modules")

# ── Final shared library (.bmodp) ────────────────────────────────────────────
#    BML mod entry point – assembles all modules into the loadable plugin DLL.
# ─────────────────────────────────────────────────────────────────────────────
add_library(BallanceTAS SHARED
        BMLLogSink.h
        BallanceTAS.h               BallanceTAS.cpp
)

target_include_directories(BallanceTAS
        PUBLIC
        $<BUILD_INTERFACE:${TAS_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(BallanceTAS
        PRIVATE
        tas_core
        tas_game
        tas_scripting
        tas_features
        tas_lua_bindings
        tas_ui
)

target_compile_options(BallanceTAS PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:char8_t->
)

set_target_properties(BallanceTAS PROPERTIES SUFFIX ".bmodp")

endif() # BUILD_MOD
