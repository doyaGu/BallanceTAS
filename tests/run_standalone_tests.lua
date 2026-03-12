--[[
    Standalone Lua Test Runner

    Runs BallanceTAS integration tests without the game engine.
    Uses mock_tas_api.lua to provide the `tas` global.

    Usage:
        lua tests/run_standalone_tests.lua          -- from repo root
        lua run_standalone_tests.lua                -- from tests/

    Exit code: 0 on all-pass, 1 on any failure.
]]

-- ============================================================================
--  Locate ourselves and the project root
-- ============================================================================

local function script_dir()
    local info = debug.getinfo(1, "S")
    local path = info.source:match("^@(.+)[/\\][^/\\]+$")
    return path or "."
end

local tests_dir = script_dir()
-- Normalise separators for dofile()
tests_dir = tests_dir:gsub("\\", "/")

-- ============================================================================
--  Load mock API
-- ============================================================================

dofile(tests_dir .. "/mock_tas_api.lua")
assert(tas, "mock_tas_api.lua did not create the global `tas` table")

-- Suppress auto-run inside test files (they check `_G._STANDALONE_RUNNER`)
_G._STANDALONE_RUNNER = true

-- ============================================================================
--  Test file list
-- ============================================================================

local test_files = {
    tests_dir .. "/test_core.lua",
    tests_dir .. "/test_savestate.lua",
}

-- ============================================================================
--  Run
-- ============================================================================

local total_files   = 0
local total_tests   = 0
local total_passed  = 0
local total_failed  = 0
local file_results  = {}

for _, filepath in ipairs(test_files) do
    total_files = total_files + 1

    -- Reset mock state between files
    tas._mock.reset()

    print(string.rep("=", 72))
    print("Running: " .. filepath)
    print(string.rep("=", 72))

    local load_ok, test_module = pcall(dofile, filepath)

    if not load_ok then
        io.stderr:write("[ERROR] Failed to load " .. filepath .. ": " .. tostring(test_module) .. "\n")
        total_failed = total_failed + 1
        table.insert(file_results, { file = filepath, error = tostring(test_module) })
    else
        -- The test files return a table with a run() method.
        local run_ok, results = pcall(test_module.run)

        if not run_ok then
            io.stderr:write("[ERROR] Failed to run " .. filepath .. ": " .. tostring(results) .. "\n")
            total_failed = total_failed + 1
            table.insert(file_results, { file = filepath, error = tostring(results) })
        else
            total_tests  = total_tests  + (results.total  or 0)
            total_passed = total_passed + (results.passed or 0)
            total_failed = total_failed + (results.failed or 0)
            table.insert(file_results, {
                file   = filepath,
                total  = results.total,
                passed = results.passed,
                failed = results.failed,
            })
        end
    end
end

-- ============================================================================
--  Summary
-- ============================================================================

print()
print(string.rep("#", 72))
print("STANDALONE TEST SUMMARY")
print(string.rep("#", 72))
print()

for _, r in ipairs(file_results) do
    if r.error then
        io.stderr:write(string.format("[ERROR] %s — %s\n", r.file, r.error))
    elseif r.failed == 0 then
        print(string.format("[PASS]  %s — %d/%d", r.file, r.passed, r.total))
    else
        io.stderr:write(string.format("[FAIL]  %s — %d/%d passed, %d failed\n",
            r.file, r.passed, r.total, r.failed))
    end
end

print()
print(string.format("Files: %d  Tests: %d  Passed: %d  Failed: %d",
    total_files, total_tests, total_passed, total_failed))

if total_failed > 0 then
    print("\n*** SOME TESTS FAILED ***")
    os.exit(1)
else
    print("\n*** ALL TESTS PASSED ***")
    os.exit(0)
end
