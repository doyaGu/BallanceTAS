# =============================================================================
# BallanceTAS – Test Suite
#
# Standalone tests (ResultTest, RuntimeSessionStateTest, LoggerTest) link only against
# tas_core which is pure C++ — no VirtoolsSDK/BML required.
#
# Full integration tests (LuaApiTest) require the complete mod build and are
# only compiled when BUILD_MOD is ON.
#
# Lua integration tests can be run standalone via the mock harness using any
# standard Lua 5.4 interpreter (see run_standalone_tests.lua).
# =============================================================================

# CTest is enabled from the top-level CMakeLists.txt

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------------------------
#  Helper – register a C++ test executable
# ---------------------------------------------------------------------------
function(add_tas_test test_name)
    cmake_parse_arguments(TEST "" "" "SOURCES;DEPENDENCIES" ${ARGN})

    add_executable(${test_name} ${TEST_SOURCES})

    target_include_directories(${test_name} PRIVATE
            "${TAS_INCLUDE_DIR}"
            "${TAS_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}"          # for ConsoleLogSink.h etc.
    )

    target_link_libraries(${test_name} PRIVATE
            gtest_main
            gmock
            ${TEST_DEPENDENCIES}
    )

    set_target_properties(${test_name} PROPERTIES FOLDER "Tests")
endfunction()

# ===========================================================================
#  Standalone C++ tests (no SDK required)
# ===========================================================================

# ResultTest – tests for Result<T> error handling (header-only, pure C++)
add_tas_test(ResultTest
    SOURCES
        ResultTest.cpp
    DEPENDENCIES
        tas_core
)

add_tas_test(RuntimeSessionStateTest
    SOURCES
        RuntimeSessionStateTest.cpp
    DEPENDENCIES
        tas_core
)

add_tas_test(RuntimeSessionLifecycleTest
    SOURCES
        RuntimeSessionLifecycleTest.cpp
    DEPENDENCIES
        tas_core
)

# LoggerTest – validates Logger + ILogSink decoupling with ConsoleLogSink
add_tas_test(LoggerTest
    SOURCES
        LoggerTest.cpp
    DEPENDENCIES
        tas_core
)

# ResourceManagerTest – RAII / temp-file tests (pure C++)
add_tas_test(ResourceManagerTest
    SOURCES
        ResourceManagerTest.cpp
    DEPENDENCIES
        tas_core
)

# EventBusTest – type-safe pub/sub with RAII subscription guards (pure C++)
add_tas_test(EventBusTest
    SOURCES
        EventBusTest.cpp
    DEPENDENCIES
        tas_core
)

add_tas_test(EventManagerTest
    SOURCES
        EventManagerTest.cpp
        ../src/Scripting/EventManager.cpp
    DEPENDENCIES
        tas_core
        tas_lua_runtime
)

add_tas_test(SharedDataManagerTest
    SOURCES
        SharedDataManagerTest.cpp
        ../src/Scripting/SharedDataManager.cpp
    DEPENDENCIES
        tas_core
        tas_lua_runtime
)

add_tas_test(MessageBusTest
    SOURCES
        MessageBusTest.cpp
        ../src/Scripting/MessageBus.cpp
    DEPENDENCIES
        tas_core
        tas_lua_runtime
)

add_tas_test(AsyncTaskTest
    SOURCES
        AsyncTaskTest.cpp
        ../src/Scripting/AsyncTask.cpp
    DEPENDENCIES
        tas_core
        tas_lua_runtime
)

add_tas_test(DeterminismTraceTest
    SOURCES
        DeterminismTraceTest.cpp
    DEPENDENCIES
        tas_core
)

# LuaRuntimeTest - validates the project-owned Lua C API abstraction layer
add_tas_test(LuaRuntimeTest
    SOURCES
        LuaRuntimeTest.cpp
    DEPENDENCIES
        tas_core
        tas_lua_runtime
)

# Register standalone tests with CTest
add_test(NAME ResultTest          COMMAND ResultTest)
add_test(NAME RuntimeSessionStateTest COMMAND RuntimeSessionStateTest)
add_test(NAME RuntimeSessionLifecycleTest COMMAND RuntimeSessionLifecycleTest)
add_test(NAME LoggerTest          COMMAND LoggerTest)
add_test(NAME ResourceManagerTest COMMAND ResourceManagerTest)
add_test(NAME EventBusTest        COMMAND EventBusTest)
add_test(NAME EventManagerTest    COMMAND EventManagerTest)
add_test(NAME SharedDataManagerTest COMMAND SharedDataManagerTest)
add_test(NAME MessageBusTest      COMMAND MessageBusTest)
add_test(NAME AsyncTaskTest       COMMAND AsyncTaskTest)
add_test(NAME DeterminismTraceTest COMMAND DeterminismTraceTest)
add_test(NAME LuaRuntimeTest      COMMAND LuaRuntimeTest)

if(BUILD_MOD)
    add_tas_test(ScriptContextManagerTest
        SOURCES
            ScriptContextManagerTest.cpp
        DEPENDENCIES
            tas_core
            tas_game
            tas_lua_runtime
    )
    add_test(NAME ScriptContextManagerTest COMMAND ScriptContextManagerTest)
    set_tests_properties(ScriptContextManagerTest PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:CK2>;PATH=path_list_prepend:$<TARGET_FILE_DIR:VxMath>;PATH=path_list_prepend:$<TARGET_FILE_DIR:BML>"
    )

    add_tas_test(InputSystemTest
        SOURCES
            InputSystemTest.cpp
        DEPENDENCIES
            tas_game
    )
    add_test(NAME InputSystemTest COMMAND InputSystemTest)
    set_tests_properties(InputSystemTest PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:CK2>;PATH=path_list_prepend:$<TARGET_FILE_DIR:VxMath>;PATH=path_list_prepend:$<TARGET_FILE_DIR:BML>"
    )

    add_tas_test(RecordFileIOTest
        SOURCES
            RecordFileIOTest.cpp
        DEPENDENCIES
            tas_features
            tas_lua_runtime
    )
    add_test(NAME RecordFileIOTest COMMAND RecordFileIOTest)
    if(VIRTOOLS_SDK_RUNTIME_DIR)
        set_tests_properties(RecordFileIOTest PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${VIRTOOLS_SDK_RUNTIME_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:CK2>;PATH=path_list_prepend:$<TARGET_FILE_DIR:VxMath>;PATH=path_list_prepend:$<TARGET_FILE_DIR:BML>"
        )
    else()
        set_tests_properties(RecordFileIOTest PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:CK2>;PATH=path_list_prepend:$<TARGET_FILE_DIR:VxMath>;PATH=path_list_prepend:$<TARGET_FILE_DIR:BML>"
        )
    endif()

    add_tas_test(SharedBufferTest
        SOURCES
            SharedBufferTest.cpp
            ../src/LuaApi/LuaApi_SharedBuffer.cpp
        DEPENDENCIES
            tas_core
            tas_lua_runtime
    )
    add_test(NAME SharedBufferTest COMMAND SharedBufferTest)
    set_tests_properties(SharedBufferTest PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:CK2>;PATH=path_list_prepend:$<TARGET_FILE_DIR:VxMath>;PATH=path_list_prepend:$<TARGET_FILE_DIR:BML>"
    )

    add_tas_test(LuaApiMathTest
        SOURCES
            LuaApiMathTest.cpp
            ../src/LuaApi/LuaApi_VxVector.cpp
            ../src/LuaApi/LuaApi_VxColor.cpp
            ../src/LuaApi/LuaApi_VxRect.cpp
            ../src/LuaApi/LuaApi_VxQuaternion.cpp
            ../src/LuaApi/LuaApi_VxMatrix.cpp
        DEPENDENCIES
            tas_core
            tas_game
            tas_lua_runtime
    )
    add_test(NAME LuaApiMathTest COMMAND LuaApiMathTest)
    if(VIRTOOLS_SDK_RUNTIME_DIR)
        set_tests_properties(LuaApiMathTest PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${VIRTOOLS_SDK_RUNTIME_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:CK2>;PATH=path_list_prepend:$<TARGET_FILE_DIR:VxMath>;PATH=path_list_prepend:$<TARGET_FILE_DIR:BML>"
        )
    else()
        set_tests_properties(LuaApiMathTest PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:CK2>;PATH=path_list_prepend:$<TARGET_FILE_DIR:VxMath>;PATH=path_list_prepend:$<TARGET_FILE_DIR:BML>"
        )
    endif()

    add_tas_test(LuaSchedulerTest
        SOURCES
            LuaSchedulerTest.cpp
            ../src/LuaApi/LuaApi_Async.cpp
            ../src/LuaApi/LuaApi_Concurrency.cpp
        DEPENDENCIES
            tas_core
            tas_game
            tas_scripting
            tas_lua_runtime
    )
    add_test(NAME LuaSchedulerTest COMMAND LuaSchedulerTest)
    set_tests_properties(LuaSchedulerTest PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:CK2>;PATH=path_list_prepend:$<TARGET_FILE_DIR:VxMath>;PATH=path_list_prepend:$<TARGET_FILE_DIR:BML>"
    )

    add_tas_test(TASMenuPresentationTest
        SOURCES
            TASMenuPresentationTest.cpp
        DEPENDENCIES
            tas_core
            tas_game
            tas_lua_runtime
            tas_ui
    )
    add_test(NAME TASMenuPresentationTest COMMAND TASMenuPresentationTest)
    if(VIRTOOLS_SDK_RUNTIME_DIR)
        set_tests_properties(TASMenuPresentationTest PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${VIRTOOLS_SDK_RUNTIME_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:CK2>;PATH=path_list_prepend:$<TARGET_FILE_DIR:VxMath>;PATH=path_list_prepend:$<TARGET_FILE_DIR:BML>"
        )
    else()
        set_tests_properties(TASMenuPresentationTest PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:CK2>;PATH=path_list_prepend:$<TARGET_FILE_DIR:VxMath>;PATH=path_list_prepend:$<TARGET_FILE_DIR:BML>"
        )
    endif()

    add_tas_test(ScriptGeneratorTest
        SOURCES
            ScriptGeneratorTest.cpp
            ../src/Workflow/ScriptGenerationCore.cpp
            ../src/Workflow/ScriptInputTransition.cpp
        DEPENDENCIES
            tas_core
            CK2
    )
    add_test(NAME ScriptGeneratorTest COMMAND ScriptGeneratorTest)
    set_tests_properties(ScriptGeneratorTest PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:CK2>"
    )

    add_tas_test(StartupProjectManagerTest
        SOURCES
            StartupProjectManagerTest.cpp
            ../src/Workflow/TASProject.cpp
        DEPENDENCIES
            tas_core
            tas_lua_runtime
            CK2
    )
    add_test(NAME StartupProjectManagerTest COMMAND StartupProjectManagerTest)
    if(VIRTOOLS_SDK_RUNTIME_DIR)
        set_tests_properties(StartupProjectManagerTest PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${VIRTOOLS_SDK_RUNTIME_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:CK2>;PATH=path_list_prepend:$<TARGET_FILE_DIR:VxMath>;PATH=path_list_prepend:$<TARGET_FILE_DIR:BML>"
        )
    else()
        set_tests_properties(StartupProjectManagerTest PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:CK2>;PATH=path_list_prepend:$<TARGET_FILE_DIR:VxMath>;PATH=path_list_prepend:$<TARGET_FILE_DIR:BML>"
        )
    endif()
endif()

# ===========================================================================
#  Standalone Lua integration tests (requires a Lua interpreter on PATH)
# ===========================================================================
find_program(LUA_INTERPRETER NAMES lua lua5.4 lua54 luajit)
if(LUA_INTERPRETER)
    add_test(
        NAME LuaStandaloneTests
        COMMAND ${LUA_INTERPRETER} "${CMAKE_CURRENT_SOURCE_DIR}/run_standalone_tests.lua"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    # The mock harness writes warnings/errors to stderr which is normal.
    # CTest uses the exit code to determine pass/fail.
    set_tests_properties(LuaStandaloneTests PROPERTIES
        ENVIRONMENT "LUA_PATH=${CMAKE_CURRENT_SOURCE_DIR}/?.lua"
    )
    message(STATUS "Lua interpreter found: ${LUA_INTERPRETER} — LuaStandaloneTests enabled")
else()
    message(STATUS "No Lua interpreter found — LuaStandaloneTests disabled (install Lua 5.4 to enable)")
endif()

# ===========================================================================
#  Full integration tests (require BUILD_MOD = ON)
# ===========================================================================
# TODO: LuaApiTest needs rewrite for ServiceProvider-based architecture
# if(BUILD_MOD)
#     add_tas_test(LuaApiTest
#         SOURCES
#             LuaApiTest.cpp
#         DEPENDENCIES
#             tas_core
#             tas_game
#             tas_scripting
#             tas_features
#             tas_lua_bindings
#     )
#     add_test(NAME LuaApiTest COMMAND LuaApiTest)
# endif()
