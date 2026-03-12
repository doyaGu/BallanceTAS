# =============================================================================
# BallanceTAS – Test Suite
#
# Standalone tests (ResultTest, StateMachineTest, LoggerTest) link only against
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

# StateMachineTest – tests for TASStateMachine (links tas_core library)
add_tas_test(StateMachineTest
    SOURCES
        StateMachineTest.cpp
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

# Register standalone tests with CTest
add_test(NAME ResultTest          COMMAND ResultTest)
add_test(NAME StateMachineTest    COMMAND StateMachineTest)
add_test(NAME LoggerTest          COMMAND LoggerTest)
add_test(NAME ResourceManagerTest COMMAND ResourceManagerTest)

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
if(BUILD_MOD)
    add_tas_test(LuaApiTest
        SOURCES
            LuaApiTest.cpp
        DEPENDENCIES
            tas_core
            tas_game
            tas_scripting
            tas_features
            tas_lua_bindings
    )
    add_test(NAME LuaApiTest COMMAND LuaApiTest)
endif()