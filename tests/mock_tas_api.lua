--[[
    Standalone Mock TAS API for running integration tests without the game.

    This module creates a `tas` global table that stubs out every API used
    by test_core.lua and test_savestate.lua.  Behaviours are simplified:
      - Timing functions use a synthetic tick counter.
      - Logging goes to stdout via print().
      - Savestate operations work in-memory.
      - Level / ball queries return dummy data.

    Usage (from the standalone runner):
        dofile("tests/mock_tas_api.lua")
        -- then load the test files normally
]]

-- ============================================================================
--  Internal state
-- ============================================================================
local _tick = 0
local _frame = 0
local _savestates = {}  -- name -> { description, tick, data }
local _events = {}      -- name -> list of callbacks
local _level_loaded = false

-- ============================================================================
--  Helpers
-- ============================================================================

--- Simple string-format that replaces {} placeholders (like fmt / Python style)
local function fmt(s, ...)
    local args = { ... }
    local i = 0
    return (s:gsub("{[^}]*}", function()
        i = i + 1
        local v = args[i]
        if v == nil then return "nil" end
        return tostring(v)
    end))
end

-- ============================================================================
--  tas namespace
-- ============================================================================

tas = {}

-- ── Logging ─────────────────────────────────────────────────────────────────

function tas.log(s, ...)
    if select("#", ...) > 0 then s = fmt(s, ...) end
    print("[TAS LOG] " .. tostring(s))
end

function tas.warn(s, ...)
    if select("#", ...) > 0 then s = fmt(s, ...) end
    io.stderr:write("[TAS WARN] " .. tostring(s) .. "\n")
end

function tas.error(s, ...)
    if select("#", ...) > 0 then s = fmt(s, ...) end
    io.stderr:write("[TAS ERROR] " .. tostring(s) .. "\n")
end

function tas.print(s, ...)
    if select("#", ...) > 0 then s = fmt(s, ...) end
    print(tostring(s))
end

-- ── Tick / Frame ────────────────────────────────────────────────────────────

function tas.get_tick()
    return _tick
end

function tas.get_frame_count()
    return _frame
end

--- Advance synthetic time by `n` ticks (simulates game ticks).
function tas.wait(n)
    n = n or 1
    _tick = _tick + n
    _frame = _frame + n
end

function tas.wait_ticks(n)
    tas.wait(n)
end

function tas.wait_until(predicate)
    -- Run predicate up to 10 000 synthetic ticks to avoid infinite loops.
    for _ = 1, 10000 do
        if predicate() then return end
        tas.wait(1)
    end
    error("tas.wait_until: timed out after 10 000 synthetic ticks")
end

-- ── Manifest ────────────────────────────────────────────────────────────────

function tas.get_manifest()
    return {
        id       = "BallanceTAS",
        version  = "2.0.0-test",
        name     = "Ballance TAS (test stub)",
        author   = "Test",
    }
end

-- ── Control flow helpers ────────────────────────────────────────────────────

function tas.sequence(...)
    for _, fn in ipairs({ ... }) do
        fn()
    end
end

function tas.repeat_count(fn, n)
    for _ = 1, n do fn() end
end

function tas.repeat_for(fn, frames)
    local target = _frame + frames
    while _frame < target do
        fn()
        tas.wait(1)
    end
end

function tas.repeat_until(fn, predicate)
    for _ = 1, 100000 do
        fn()
        if predicate() then return end
        tas.wait(1)
    end
    error("tas.repeat_until: timed out")
end

-- ── Assertions ──────────────────────────────────────────────────────────────

function tas.assert(cond, msg)
    if not cond then
        error(msg or "tas.assert failed", 2)
    end
end

-- ── Async (stub — executes synchronously in standalone mode) ────────────────

function tas.async(fn, ...)
    local args = { ... }
    local done = false
    local ok, err = pcall(fn, table.unpack(args))
    done = true
    if not ok then
        tas.error("async task error: {}", err)
    end
    return {
        is_done = function() return done end,
    }
end

-- ── Events (simplified) ────────────────────────────────────────────────────

function tas.on(event_name, callback)
    _events[event_name] = _events[event_name] or {}
    table.insert(_events[event_name], callback)
end

function tas.fire(event_name, ...)
    if _events[event_name] then
        for _, cb in ipairs(_events[event_name]) do
            cb(...)
        end
    end
end

-- ── Savestate subsystem ─────────────────────────────────────────────────────

tas.savestate = {}

function tas.savestate.save(name, description)
    _savestates[name] = {
        description = description or "",
        tick = _tick,
        data = "mock_state_data",
    }
    return nil  -- nil means success
end

function tas.savestate.load(name)
    if not _savestates[name] then
        error("savestate not found: " .. tostring(name), 2)
    end
    return nil
end

function tas.savestate.exists(name)
    return _savestates[name] ~= nil
end

function tas.savestate.remove(name)
    _savestates[name] = nil
end

function tas.savestate.list()
    local names = {}
    for k, _ in pairs(_savestates) do
        table.insert(names, k)
    end
    table.sort(names)
    return names
end

function tas.savestate.get_info(name)
    local s = _savestates[name]
    if not s then return nil end
    return {
        name = name,
        description = s.description,
        tick = s.tick,
        size = #s.data,
        level = 1,
        timestamp = os.time(),
    }
end

function tas.savestate.clear()
    _savestates = {}
end

function tas.savestate.get_directory()
    return "./savestates"
end

-- ── Level / Ball stubs ──────────────────────────────────────────────────────

tas.level = {}

function tas.level.is_loaded()
    return _level_loaded
end

function tas.get_ball_position()
    return { x = 0.0, y = 0.0, z = 0.0 }
end

-- ── Input stubs ─────────────────────────────────────────────────────────────

function tas.press(key)       end
function tas.hold(key, frames) end
function tas.key_down(key)    end
function tas.key_up(key)      end
function tas.release_all_keys() end

-- ── End script ──────────────────────────────────────────────────────────────

function tas.end_script(reason)
    tas.log("end_script called: {}", reason or "")
end

-- ── Mock control ────────────────────────────────────────────────────────────
-- Allow tests / runner to manipulate internal state.

tas._mock = {}

function tas._mock.reset()
    _tick = 0
    _frame = 0
    _savestates = {}
    _events = {}
    _level_loaded = false
end

function tas._mock.set_level_loaded(v)
    _level_loaded = v
end

function tas._mock.advance(n)
    _tick = _tick + (n or 1)
    _frame = _frame + (n or 1)
end
