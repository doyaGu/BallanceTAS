set(TAS_PUBLIC_HEADERS)

set(TAS_PRIVATE_HEADERS
        BallanceTAS.h
		TASEngine.h
		Logger.h
		InputSystem.h
		DX8InputManager.h
		EventManager.h
		GameInterface.h
        ProjectManager.h
		TASProject.h
		ScriptContext.h
		ScriptContextManager.h
		SharedDataManager.h
		MessageBus.h
		LuaScheduler.h
		LuaREPLServer.h
		RecordPlayer.h
		Recorder.h
		ScriptGenerator.h
		StartupProjectManager.h
		AsyncTask.h

		LuaApi.h

		TASHook.h
		physics_RT.h

		UIManager.h
		TASMenu.h
		InGameOSD.h

		# Phase 1 Refactoring Components
		Result.h
		TASStateMachine.h
		TASStateHandlers.h
		TASStrategies.h
		TASControllers.h
		ServiceContainer.h
		ResourceManager.h
		SavestateManager.h
)

set(TAS_SOURCES
		BallanceTAS.cpp
		TASEngine.cpp
		Logger.cpp
		InputSystem.cpp
		EventManager.cpp
		GameInterface.cpp
        ProjectManager.cpp
		TASProject.cpp
		ScriptContext.cpp
		ScriptContextManager.cpp
		SharedDataManager.cpp
		MessageBus.cpp
		LuaScheduler.cpp
		LuaREPLServer.cpp
		RecordPlayer.cpp
		Recorder.cpp
		ScriptGenerator.cpp
		StartupProjectManager.cpp
		AsyncTask.cpp

		LuaApi/LuaApi.cpp
		LuaApi/LuaApi_Core.cpp
		LuaApi/LuaApi_Debug.cpp
		LuaApi/LuaApi_Global.cpp
		LuaApi/LuaApi_GC.cpp
		LuaApi/LuaApi_Input.cpp
		LuaApi/LuaApi_World.cpp
		LuaApi/LuaApi_Concurrency.cpp
		LuaApi/LuaApi_Context.cpp
		LuaApi/LuaApi_Record.cpp
		LuaApi/LuaApi_Project.cpp
		LuaApi/LuaApi_Level.cpp
		LuaApi/LuaApi_Menu.cpp
		LuaApi/LuaApi_SharedBuffer.cpp
		LuaApi/LuaApi_Result.cpp
		LuaApi/LuaApi_Async.cpp
		LuaApi/LuaApi_VxColor.cpp
		LuaApi/LuaApi_VxMatrix.cpp
		LuaApi/LuaApi_VxQuaternion.cpp
		LuaApi/LuaApi_VxRect.cpp
		LuaApi/LuaApi_VxVector.cpp
		LuaApi/LuaApi_VxIntersectionDesc.cpp
		LuaApi/LuaApi_CKEnums.cpp
		LuaApi/LuaApi_CKObject.cpp
		LuaApi/LuaApi_CKSceneObject.cpp
		LuaApi/LuaApi_CKBeObject.cpp
		LuaApi/LuaApi_CKRenderObject.cpp
		LuaApi/LuaApi_CK3dEntity.cpp
		LuaApi/LuaApi_CKCamera.cpp
		LuaApi/LuaApi_PhysicsObject.cpp
		LuaApi/LuaApi_Savestate.cpp

        physics_RT.cpp

		UIManager.cpp
		TASMenu.cpp
		InGameOSD.cpp

		# Phase 1 Refactoring Components
		TASStateMachine.cpp
		TASStateHandlers.cpp
		TASStrategies.cpp
		TASControllers.cpp
		ServiceContainer.cpp
		ResourceManager.cpp
		SavestateManager.cpp
)

add_library(BallanceTAS SHARED
        ${TAS_SOURCES} ${TAS_PUBLIC_HEADERS} ${TAS_PRIVATE_HEADERS}
)

target_include_directories(BallanceTAS
        PUBLIC
        $<BUILD_INTERFACE:${TAS_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(BallanceTAS
        PUBLIC
		BML CK2 VxMath
        PRIVATE
		minhook lua::lua sol2 zip fmt::fmt yyjson dinput8
)

target_compile_options(BallanceTAS PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/Zc:char8_t->
)

set_target_properties(BallanceTAS PROPERTIES SUFFIX ".bmodp")
