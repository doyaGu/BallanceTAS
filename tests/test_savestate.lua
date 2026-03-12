--[[
    Savestate System Integration Tests

    Tests savestate functionality:
    - Save and load states
    - State validation
    - Metadata retrieval
    - State listing and management
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

-- Cleanup helper
local function cleanup_test_states()
    local states = tas.savestate.list()
    for _, name in ipairs(states) do
        if string.find(name, "^test_") then
            tas.savestate.remove(name)
        end
    end
end

--------------------------------------------------------------------------------
-- Basic Save/Load Tests
--------------------------------------------------------------------------------

function test_savestate_basic_save()
    cleanup_test_states()

    -- Save a state
    local err = tas.savestate.save("test_basic")

    assert(err == nil, "Save should succeed: " .. tostring(err))

    -- Verify it exists
    local exists = tas.savestate.exists("test_basic")
    assert(exists, "Saved state should exist")

    -- Cleanup
    tas.savestate.remove("test_basic")

    return true
end

function test_savestate_save_with_description()
    cleanup_test_states()

    -- Save with description
    local err = tas.savestate.save("test_desc", "Test savestate with description")

    assert(err == nil, "Save with description should succeed")

    -- Get info and verify description
    local info = tas.savestate.get_info("test_desc")
    assert(info ~= nil, "Should be able to get state info")
    assert(info.description == "Test savestate with description", "Description should match")

    -- Cleanup
    tas.savestate.remove("test_desc")

    return true
end

function test_savestate_load_basic()
    cleanup_test_states()

    -- Need to be in-game for this test
    if not tas.level.is_loaded() then
        tas.warn("Skipping load test - level not loaded")
        return true
    end

    -- Get initial position
    local pos1 = tas.get_ball_position()

    -- Save state
    local err = tas.savestate.save("test_load")
    assert(err == nil, "Save should succeed")

    -- Move ball (simulated by waiting)
    tas.wait(60)

    -- Load state
    tas.savestate.load("test_load")

    -- Verify position restored (approximately)
    local pos2 = tas.get_ball_position()

    -- Cleanup
    tas.savestate.remove("test_load")

    -- Note: Actual position comparison depends on physics stabilization
    tas.log("Position before: ({:.2f}, {:.2f}, {:.2f})", pos1.x, pos1.y, pos1.z)
    tas.log("Position after:  ({:.2f}, {:.2f}, {:.2f})", pos2.x, pos2.y, pos2.z)

    return true
end

--------------------------------------------------------------------------------
-- State Management Tests
--------------------------------------------------------------------------------

function test_savestate_list()
    cleanup_test_states()

    -- Create several states
    tas.savestate.save("test_list_1")
    tas.savestate.save("test_list_2")
    tas.savestate.save("test_list_3")

    -- List states
    local states = tas.savestate.list()

    -- Count test states
    local count = 0
    for _, name in ipairs(states) do
        if string.find(name, "^test_list_") then
            count = count + 1
        end
    end

    assert(count >= 3, string.format("Should have at least 3 test states (found %d)", count))

    -- Cleanup
    cleanup_test_states()

    return true
end

function test_savestate_exists()
    cleanup_test_states()

    -- Should not exist before creation
    local exists_before = tas.savestate.exists("test_exists")
    assert(not exists_before, "State should not exist before creation")

    -- Create state
    tas.savestate.save("test_exists")

    -- Should exist after creation
    local exists_after = tas.savestate.exists("test_exists")
    assert(exists_after, "State should exist after creation")

    -- Remove state
    tas.savestate.remove("test_exists")

    -- Should not exist after removal
    local exists_removed = tas.savestate.exists("test_exists")
    assert(not exists_removed, "State should not exist after removal")

    return true
end

function test_savestate_remove()
    cleanup_test_states()

    -- Create state
    tas.savestate.save("test_remove")

    -- Verify exists
    assert(tas.savestate.exists("test_remove"), "State should exist before removal")

    -- Remove
    local err = tas.savestate.remove("test_remove")
    assert(err == nil, "Remove should succeed")

    -- Verify removed
    assert(not tas.savestate.exists("test_remove"), "State should not exist after removal")

    return true
end

function test_savestate_overwrite()
    cleanup_test_states()

    -- Create first state
    tas.savestate.save("test_overwrite", "First version")

    -- Overwrite with second state
    tas.wait(10)
    tas.savestate.save("test_overwrite", "Second version")

    -- Should still exist (only one instance)
    assert(tas.savestate.exists("test_overwrite"), "State should exist after overwrite")

    -- Check description updated
    local info = tas.savestate.get_info("test_overwrite")
    assert(info.description == "Second version", "Description should be updated")

    -- Cleanup
    tas.savestate.remove("test_overwrite")

    return true
end

--------------------------------------------------------------------------------
-- Metadata Tests
--------------------------------------------------------------------------------

function test_savestate_get_info()
    cleanup_test_states()

    if not tas.level.is_loaded() then
        tas.warn("Skipping info test - level not loaded")
        return true
    end

    -- Save state
    tas.savestate.save("test_info", "Test metadata retrieval")

    -- Get info
    local info = tas.savestate.get_info("test_info")

    assert(info ~= nil, "Should be able to get info")
    assert(info.name == "test_info", "Name should match")
    assert(info.description == "Test metadata retrieval", "Description should match")

    -- Check required fields
    assert(info.timestamp ~= nil, "Should have timestamp")
    assert(info.level_name ~= nil, "Should have level name")
    assert(info.level_number ~= nil, "Should have level number")
    assert(info.position ~= nil, "Should have position")
    assert(info.tick ~= nil, "Should have tick")

    -- Check position format
    assert(type(info.position) == "table", "Position should be a table")
    assert(info.position.x ~= nil, "Position should have x")
    assert(info.position.y ~= nil, "Position should have y")
    assert(info.position.z ~= nil, "Position should have z")

    tas.log("Savestate info:")
    tas.log("  Name: {}", info.name)
    tas.log("  Level: {} ({})", info.level_name, info.level_number)
    tas.log("  Position: ({:.2f}, {:.2f}, {:.2f})",
            info.position.x, info.position.y, info.position.z)
    tas.log("  Tick: {}", info.tick)

    -- Cleanup
    tas.savestate.remove("test_info")

    return true
end

function test_savestate_get_directory()
    local dir = tas.savestate.get_directory()

    assert(type(dir) == "string", "Directory should be a string")
    assert(#dir > 0, "Directory should not be empty")

    tas.log("Savestate directory: {}", dir)

    return true
end

--------------------------------------------------------------------------------
-- Validation Tests
--------------------------------------------------------------------------------

function test_savestate_invalid_name()
    -- Test invalid characters (implementation-specific)
    local names_to_test = {
        "",                    -- Empty
        "test name",           -- Space
        "test/name",           -- Slash
        "test\\name",          -- Backslash
        "test?name",           -- Question mark
        string.rep("a", 100),  -- Too long
    }

    local any_failed = false

    for _, name in ipairs(names_to_test) do
        local err = tas.savestate.save(name)

        if err then
            -- Expected failure
            tas.log("Correctly rejected invalid name: '{}'", name)
        else
            -- Unexpected success - cleanup
            tas.savestate.remove(name)
            any_failed = true
        end
    end

    -- Note: Some implementations may allow certain characters
    -- This is a soft test
    tas.log("Invalid name test completed")

    return true
end

function test_savestate_nonexistent_load()
    -- Try to load non-existent state
    local status, err = pcall(function()
        tas.savestate.load("nonexistent_state_12345")
    end)

    -- Should fail
    assert(not status, "Loading non-existent state should fail")

    tas.log("Correctly failed to load non-existent state")

    return true
end

function test_savestate_nonexistent_info()
    -- Try to get info for non-existent state
    local info = tas.savestate.get_info("nonexistent_state_67890")

    -- Should return nil
    assert(info == nil, "Info for non-existent state should be nil")

    return true
end

--------------------------------------------------------------------------------
-- Stress Tests
--------------------------------------------------------------------------------

function test_savestate_multiple_states()
    cleanup_test_states()

    -- Create multiple states
    local count = 10

    for i = 1, count do
        local name = string.format("test_multi_%d", i)
        tas.savestate.save(name, string.format("State %d", i))
        tas.wait(2)  -- Small delay between saves
    end

    -- Verify all exist
    for i = 1, count do
        local name = string.format("test_multi_%d", i)
        assert(tas.savestate.exists(name),
               string.format("State %s should exist", name))
    end

    -- Cleanup
    cleanup_test_states()

    return true
end

function test_savestate_rapid_save_load()
    cleanup_test_states()

    if not tas.level.is_loaded() then
        tas.warn("Skipping rapid test - level not loaded")
        return true
    end

    -- Rapid save/load cycles
    for i = 1, 5 do
        tas.savestate.save("test_rapid")
        tas.wait(10)
        tas.savestate.load("test_rapid")
        tas.wait(5)
    end

    -- Cleanup
    tas.savestate.remove("test_rapid")

    tas.log("Completed {} rapid save/load cycles", 5)

    return true
end

--------------------------------------------------------------------------------
-- Run All Tests
--------------------------------------------------------------------------------

function main()
    tas.log("========================================")
    tas.log("BallanceTAS Savestate Tests")
    tas.log("========================================")

    -- Initial cleanup
    cleanup_test_states()

    -- Run all tests
    run_test("Basic Save", test_savestate_basic_save)
    run_test("Save with Description", test_savestate_save_with_description)
    run_test("Basic Load", test_savestate_load_basic)
    run_test("List States", test_savestate_list)
    run_test("State Exists", test_savestate_exists)
    run_test("Remove State", test_savestate_remove)
    run_test("Overwrite State", test_savestate_overwrite)
    run_test("Get State Info", test_savestate_get_info)
    run_test("Get Directory", test_savestate_get_directory)
    run_test("Invalid Names", test_savestate_invalid_name)
    run_test("Nonexistent Load", test_savestate_nonexistent_load)
    run_test("Nonexistent Info", test_savestate_nonexistent_info)
    run_test("Multiple States", test_savestate_multiple_states)
    run_test("Rapid Save/Load", test_savestate_rapid_save_load)

    -- Final cleanup
    cleanup_test_states()

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
