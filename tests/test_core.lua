--[[
    Core API Integration Tests

    Tests core BallanceTAS functionality:
    - Logging
    - Tick/Frame counting
    - Manifest access
    - Error handling
]]

local test_results = {}
local tests_run = 0
local tests_passed = 0
local tests_failed = 0

-- Helper function to run a test
local function run_test(test_name, test_func)
    tests_run = tests_run + 1

    tas.log("[TEST] Running: {}", test_name)

    local status, err = pcall(test_func)

    if status then
        tests_passed = tests_passed + 1
        tas.log("[PASS] {}", test_name)
        table.insert(test_results, {name = test_name, status = "PASS"})
        return true
    else
        tests_failed = tests_failed + 1
        tas.error("[FAIL] {}: {}", test_name, err)
        table.insert(test_results, {name = test_name, status = "FAIL", error = err})
        return false
    end
end

--------------------------------------------------------------------------------
-- Logging Tests
--------------------------------------------------------------------------------

function test_log_functions()
    -- Test that logging functions don't crash
    tas.log("Test log message")
    tas.warn("Test warning message")
    tas.error("Test error message")  -- Note: doesn't throw, just logs
    tas.print("Test print message")

    -- Test formatted logging
    tas.log("Format test: {}, {}, {}", 1, "two", 3.0)
    tas.log("Position test: ({:.2f}, {:.2f}, {:.2f})", 1.234, 5.678, 9.012)

    return true
end

function test_log_edge_cases()
    -- Test with special characters
    tas.log("Special chars: {} [] () <> /\\")

    -- Test with nil
    tas.log("Nil test: {}", nil)

    -- Test with boolean
    tas.log("Bool test: {} and {}", true, false)

    -- Test with empty string
    tas.log("Empty: '{}'", "")

    return true
end

--------------------------------------------------------------------------------
-- Tick/Frame Tests
--------------------------------------------------------------------------------

function test_get_tick()
    local tick1 = tas.get_tick()
    assert(type(tick1) == "number", "get_tick should return number")
    assert(tick1 >= 0, "Tick should be non-negative")

    -- Wait and check tick increased
    tas.wait(5)

    local tick2 = tas.get_tick()
    assert(tick2 > tick1, "Tick should increase after waiting")
    assert(tick2 - tick1 >= 5, "Tick should increase by at least wait time")

    return true
end

function test_get_frame_count()
    local frame1 = tas.get_frame_count()
    assert(type(frame1) == "number", "get_frame_count should return number")
    assert(frame1 >= 0, "Frame count should be non-negative")

    -- Wait and check frame increased
    tas.wait(10)

    local frame2 = tas.get_frame_count()
    assert(frame2 > frame1, "Frame count should increase after waiting")
    assert(frame2 - frame1 >= 10, "Frame count should increase by at least wait time")

    return true
end

--------------------------------------------------------------------------------
-- Manifest Tests
--------------------------------------------------------------------------------

function test_get_manifest()
    local manifest = tas.get_manifest()

    assert(type(manifest) == "table", "Manifest should be a table")

    -- Check for expected fields (implementation-specific)
    -- This is a basic check - actual fields depend on implementation
    assert(manifest ~= nil, "Manifest should not be nil")

    return true
end

--------------------------------------------------------------------------------
-- Wait/Timing Tests
--------------------------------------------------------------------------------

function test_wait_basic()
    local start_frame = tas.get_frame_count()

    tas.wait(30)

    local end_frame = tas.get_frame_count()
    local elapsed = end_frame - start_frame

    assert(elapsed >= 30, string.format("Should wait at least 30 frames (waited %d)", elapsed))
    assert(elapsed <= 35, string.format("Should not wait too much longer (waited %d)", elapsed))

    return true
end

function test_wait_ticks()
    local start_tick = tas.get_tick()

    tas.wait_ticks(20)

    local end_tick = tas.get_tick()
    local elapsed = end_tick - start_tick

    assert(elapsed >= 20, string.format("Should wait at least 20 ticks (waited %d)", elapsed))

    return true
end

function test_wait_until()
    local target_frame = tas.get_frame_count() + 25

    tas.wait_until(function()
        return tas.get_frame_count() >= target_frame
    end)

    local actual_frame = tas.get_frame_count()

    assert(actual_frame >= target_frame, "wait_until should wait until condition is true")

    return true
end

--------------------------------------------------------------------------------
-- Sequence/Control Flow Tests
--------------------------------------------------------------------------------

function test_sequence()
    local results = {}

    local task1 = function()
        table.insert(results, 1)
        tas.wait(5)
    end

    local task2 = function()
        table.insert(results, 2)
        tas.wait(5)
    end

    local task3 = function()
        table.insert(results, 3)
        tas.wait(5)
    end

    tas.sequence(task1, task2, task3)

    assert(#results == 3, "All tasks should execute")
    assert(results[1] == 1, "Task 1 should run first")
    assert(results[2] == 2, "Task 2 should run second")
    assert(results[3] == 3, "Task 3 should run third")

    return true
end

function test_repeat_count()
    local counter = 0

    tas.repeat_count(function()
        counter = counter + 1
    end, 10)

    assert(counter == 10, string.format("Should repeat exactly 10 times (got %d)", counter))

    return true
end

function test_repeat_for()
    local start_frame = tas.get_frame_count()
    local counter = 0

    tas.repeat_for(function()
        counter = counter + 1
    end, 30)

    local end_frame = tas.get_frame_count()

    assert(counter > 0, "Should execute at least once")
    assert(end_frame - start_frame >= 30, "Should repeat for at least 30 frames")

    return true
end

function test_repeat_until()
    local counter = 0

    tas.repeat_until(function()
        counter = counter + 1
    end, function()
        return counter >= 5
    end)

    assert(counter >= 5, "Should repeat until condition is met")

    return true
end

--------------------------------------------------------------------------------
-- Error Handling Tests
--------------------------------------------------------------------------------

function test_assert_basic()
    -- This should not fail
    tas.assert(true, "This assertion should pass")

    -- Test with truthy values
    tas.assert(1, "Number should be truthy")
    tas.assert("string", "String should be truthy")
    tas.assert({}, "Table should be truthy")

    return true
end

function test_assert_fail()
    -- Test that assert throws error
    local status, err = pcall(function()
        tas.assert(false, "Expected failure")
    end)

    assert(status == false, "Assert with false should throw error")

    return true
end

function test_pcall_error_recovery()
    local success = false

    -- Test error recovery
    local status, err = pcall(function()
        error("Intentional error for testing")
    end)

    if not status then
        -- Error was caught
        success = true
    end

    assert(success, "Should be able to catch and recover from errors")

    return true
end

--------------------------------------------------------------------------------
-- Run All Tests
--------------------------------------------------------------------------------

function main()
    tas.log("========================================")
    tas.log("BallanceTAS Core API Tests")
    tas.log("========================================")

    -- Run all tests
    run_test("Logging Functions", test_log_functions)
    run_test("Logging Edge Cases", test_log_edge_cases)
    run_test("Get Tick", test_get_tick)
    run_test("Get Frame Count", test_get_frame_count)
    run_test("Get Manifest", test_get_manifest)
    run_test("Wait Basic", test_wait_basic)
    run_test("Wait Ticks", test_wait_ticks)
    run_test("Wait Until", test_wait_until)
    run_test("Sequence", test_sequence)
    run_test("Repeat Count", test_repeat_count)
    run_test("Repeat For", test_repeat_for)
    run_test("Repeat Until", test_repeat_until)
    run_test("Assert Basic", test_assert_basic)
    run_test("Assert Fail", test_assert_fail)
    run_test("PCAll Error Recovery", test_pcall_error_recovery)

    -- Print summary
    tas.log("\n========================================")
    tas.log("Test Summary")
    tas.log("========================================")
    tas.log("Total:  {}", tests_run)
    tas.log("Passed: {} ({:.1f}%)", tests_passed, (tests_passed / tests_run) * 100)
    tas.log("Failed: {} ({:.1f}%)", tests_failed, (tests_failed / tests_run) * 100)
    tas.log("========================================")

    -- Return results for test runner
    return {
        total = tests_run,
        passed = tests_passed,
        failed = tests_failed,
        results = test_results
    }
end

-- Auto-run if executed directly (not via standalone runner)
if not ... and not _G._STANDALONE_RUNNER then
    main()
end

return {
    run = main
}
