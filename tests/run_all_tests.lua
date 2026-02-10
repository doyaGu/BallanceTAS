--[[
    BallanceTAS Test Runner

    Runs all integration tests and generates a summary report.

    Usage:
        dofile("tests/run_all_tests.lua")
]]

local all_results = {}
local total_tests = 0
local total_passed = 0
local total_failed = 0

-- Test files to run
local test_files = {
    "tests/test_core.lua",
    "tests/test_savestate.lua",
    -- Add more test files here as they are created
    -- "tests/test_input.lua",
    -- "tests/test_async.lua",
    -- "tests/test_recording.lua",
    -- "tests/test_context_comm.lua",
    -- "tests/test_rng.lua",
}

-- Helper function to load and run a test file
local function run_test_file(filepath)
    tas.log("\n")
    tas.log("================================================================================")
    tas.log("Running: {}", filepath)
    tas.log("================================================================================")

    local status, test_module = pcall(dofile, filepath)

    if not status then
        tas.error("Failed to load test file {}: {}", filepath, test_module)
        return {
            file = filepath,
            total = 0,
            passed = 0,
            failed = 1,
            error = test_module
        }
    end

    -- Run the test module
    local run_status, results = pcall(test_module.run)

    if not run_status then
        tas.error("Failed to run tests in {}: {}", filepath, results)
        return {
            file = filepath,
            total = 0,
            passed = 0,
            failed = 1,
            error = results
        }
    end

    -- Add filename to results
    results.file = filepath

    return results
end

-- Main function
function main()
    tas.log("################################################################################")
    tas.log("#                                                                              #")
    tas.log("#                   BallanceTAS Integration Test Suite                        #")
    tas.log("#                                                                              #")
    tas.log("################################################################################")

    local start_tick = tas.get_tick()

    -- Run all test files
    for _, filepath in ipairs(test_files) do
        local results = run_test_file(filepath)

        table.insert(all_results, results)

        total_tests = total_tests + (results.total or 0)
        total_passed = total_passed + (results.passed or 0)
        total_failed = total_failed + (results.failed or 0)

        -- Wait a bit between test files
        tas.wait(30)
    end

    local end_tick = tas.get_tick()
    local duration_ticks = end_tick - start_tick
    local duration_seconds = duration_ticks / 30  -- Assuming 30 ticks/second

    -- Print overall summary
    tas.log("\n")
    tas.log("################################################################################")
    tas.log("#                          OVERALL TEST SUMMARY                               #")
    tas.log("################################################################################")
    tas.log("")

    -- Per-file summary
    tas.log("Results by Test File:")
    tas.log("---")
    for _, results in ipairs(all_results) do
        local filename = results.file:match("([^/]+)$")
        local pass_rate = 0
        if results.total > 0 then
            pass_rate = (results.passed / results.total) * 100
        end

        if results.failed == 0 and results.total > 0 then
            tas.log("[PASS] {} - {}/{} tests passed ({:.1f}%)",
                    filename, results.passed, results.total, pass_rate)
        elseif results.error then
            tas.error("[ERROR] {} - Failed to run: {}", filename, results.error)
        else
            tas.error("[FAIL] {} - {}/{} tests passed ({:.1f}%), {} failed",
                     filename, results.passed, results.total, pass_rate, results.failed)
        end
    end

    tas.log("")
    tas.log("---")
    tas.log("Overall Statistics:")
    tas.log("  Total Test Files:  {}", #test_files)
    tas.log("  Total Tests Run:   {}", total_tests)
    tas.log("  Tests Passed:      {} ({:.1f}%)", total_passed,
            total_tests > 0 and (total_passed / total_tests) * 100 or 0)
    tas.log("  Tests Failed:      {} ({:.1f}%)", total_failed,
            total_tests > 0 and (total_failed / total_tests) * 100 or 0)
    tas.log("  Duration:          {:.2f} seconds ({} ticks)", duration_seconds, duration_ticks)
    tas.log("")

    -- Final verdict
    if total_failed == 0 and total_tests > 0 then
        tas.log("################################################################################")
        tas.log("#                          ALL TESTS PASSED!                                  #")
        tas.log("################################################################################")
    elseif total_tests == 0 then
        tas.warn("################################################################################")
        tas.warn("#                          NO TESTS WERE RUN                                  #")
        tas.warn("################################################################################")
    else
        tas.error("################################################################################")
        tas.error("#                      SOME TESTS FAILED                                      #")
        tas.error("################################################################################")
    end

    -- Return summary for external callers
    return {
        total_files = #test_files,
        total_tests = total_tests,
        total_passed = total_passed,
        total_failed = total_failed,
        duration_seconds = duration_seconds,
        file_results = all_results
    }
end

-- Run tests
local final_results = main()

-- Export results for CI/CD if needed
_G.test_results = final_results
