include(CTest)
enable_testing()

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

function(add_tas_test test_name)
    cmake_parse_arguments(TEST "" "" "SOURCES;DEPENDENCIES" ${ARGN})

    add_executable(${test_name} ${TEST_SOURCES})

    target_include_directories(${test_name} PRIVATE
            "${TAS_INCLUDE_DIR}"
            "${TAS_SOURCE_DIR}"
    )

    target_link_libraries(${test_name} PRIVATE
            gtest_main
            gmock
            ${TEST_DEPENDENCIES}
    )

    set_target_properties(${test_name} PROPERTIES FOLDER "Tests")
endfunction()

# ResultTest - Tests for Result<T> error handling
add_tas_test(ResultTest
    SOURCES
    ResultTest.cpp
)

# StateMachineTest - Tests for TASStateMachine
add_tas_test(StateMachineTest
    SOURCES
    StateMachineTest.cpp
    ${TAS_SOURCE_DIR}/TASStateMachine.cpp
    ${TAS_SOURCE_DIR}/ResourceManager.cpp
)

# Register with CTest
add_test(NAME ResultTest COMMAND ResultTest)
add_test(NAME StateMachineTest COMMAND StateMachineTest)