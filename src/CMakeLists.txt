set(TAS_PUBLIC_HEADERS
        # ${TAS_INCLUDE_DIR}/Export.h
)

set(TAS_PRIVATE_HEADERS
        BallanceTAS.h
		TASEngine.h
		InputSystem.h
		EventManager.h
		GameInterface.h
		DevTools.h
		LuaApi.h
		LuaScheduler.h
		ProjectManager.h
		TASProject.h
		Recorder.h
		ScriptGenerator.h

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
		LuaApi.cpp
		LuaScheduler.cpp
		ProjectManager.cpp
		TASProject.cpp
		Recorder.cpp
		ScriptGenerator.cpp

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
		minhook lua::lua sol2
)

target_compile_options(BallanceTAS PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/Zc:char8_t->
)

set_target_properties(BallanceTAS PROPERTIES SUFFIX ".bmodp")
