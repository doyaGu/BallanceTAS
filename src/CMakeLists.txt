set(TAS_PUBLIC_HEADERS)

set(TAS_PRIVATE_HEADERS
        BallanceTAS.h
		TASEngine.h
		InputSystem.h
		EventManager.h
		GameInterface.h
		DevTools.h
		ProjectManager.h
		TASProject.h
		Recorder.h
		ScriptGenerator.h
		LuaScheduler.h

		LuaApi.h

		TASHook.h
		physics_RT.h

		UIManager.h
		TASMenu.h
		InGameOSD.h
)

set(TAS_SOURCES
		BallanceTAS.cpp
		TASEngine.cpp
		InputSystem.cpp
		EventManager.cpp
		GameInterface.cpp
		DevTools.cpp
		ProjectManager.cpp
		TASProject.cpp
		Recorder.cpp
		ScriptGenerator.cpp
		LuaScheduler.cpp

		LuaApi.cpp
		LuaApi_TAS.cpp
		LuaApi_VxColor.cpp
		LuaApi_VxMatrix.cpp
		LuaApi_VxQuaternion.cpp
		LuaApi_VxRect.cpp
		LuaApi_VxVector.cpp
		LuaApi_CKObject.cpp
		LuaApi_CKSceneObject.cpp
		LuaApi_CKBeObject.cpp
		LuaApi_CKScene.cpp
		LuaApi_CKLevel.cpp
		LuaApi_CKGroup.cpp
		LuaApi_CKMaterial.cpp
		LuaApi_CKTexture.cpp
		LuaApi_CKMesh.cpp
		LuaApi_CKRenderObject.cpp
		LuaApi_CK2dEntity.cpp
		LuaApi_CK3dEntity.cpp
		LuaApi_CKCamera.cpp
		LuaApi_CKTargetCamera.cpp
		LuaApi_CKPlace.cpp
		LuaApi_CKLight.cpp
		LuaApi_CKTargetLight.cpp

		TASHook.cpp
		physics_RT.cpp

		UIManager.cpp
		TASMenu.cpp
		InGameOSD.cpp
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
		minhook lua::lua sol2 zip
)

target_compile_options(BallanceTAS PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/Zc:char8_t->
)

set_target_properties(BallanceTAS PROPERTIES SUFFIX ".bmodp")
