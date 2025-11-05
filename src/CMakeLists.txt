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
		ResourceManager.h
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

		LuaApi.cpp
		LuaApi_Core.cpp
		LuaApi_Debug.cpp
		LuaApi_Global.cpp
		LuaApi_GC.cpp
		LuaApi_Input.cpp
		LuaApi_World.cpp
		LuaApi_Concurrency.cpp
		LuaApi_Context.cpp
		LuaApi_Record.cpp
		LuaApi_Project.cpp
		LuaApi_Level.cpp
		LuaApi_Menu.cpp
		LuaApi_SharedBuffer.cpp
		LuaApi_Result.cpp
		LuaApi_Async.cpp
		LuaApi_VxColor.cpp
		LuaApi_VxMatrix.cpp
		LuaApi_VxQuaternion.cpp
		LuaApi_VxRect.cpp
		LuaApi_VxVector.cpp
		LuaApi_VxIntersectionDesc.cpp
		LuaApi_CKEnums.cpp
		LuaApi_CKObject.cpp
		LuaApi_CKSceneObject.cpp
		LuaApi_CKBeObject.cpp
		LuaApi_CKRenderObject.cpp
		LuaApi_CK3dEntity.cpp
		LuaApi_CKCamera.cpp
		LuaApi_PhysicsObject.cpp

        physics_RT.cpp

		UIManager.cpp
		TASMenu.cpp
		InGameOSD.cpp

		# Phase 1 Refactoring Components
		TASStateMachine.cpp
		TASStateHandlers.cpp
		TASStrategies.cpp
		TASControllers.cpp
		ResourceManager.cpp
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
