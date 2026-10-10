local util = require("smoke_util")
local check, expect_ok, wait_for, listen, wait_screen =
  util.check, util.expect_ok, util.wait_for, util.listen, util.wait_screen

-- Record -> script -> record closes the loop when the inputs and frame times come back
-- unchanged and the script plays the record's physics bit for bit. Runs the verified SR_01
-- run through to the finish, then a copy with changed frame times.
local TAS_DIR = "../ModLoader/TAS/"
local SOURCE = TAS_DIR .. "SR_01_1.16.726.tas"

local function remove_project(name)
  os.remove(TAS_DIR .. name .. ".tas")
  os.remove(TAS_DIR .. name .. ".tas.notes.json")
  local dir = (TAS_DIR .. name):gsub("/", "\\")
  -- Generated script directories also hold the dumps of their playbacks.
  if os.remove(dir .. "\\manifest.lua") then
    os.execute('rmdir /s /q "' .. dir .. '" 2>nul')
  end
end

local function wait_listed(name)
  wait_for("project " .. name .. " to be listed", function()
    return tas.project.find(name) ~= nil
  end, 15)
end

-- A copy of SOURCE, changed by `edit` before saving.
local function make_record(name, edit)
  remove_project(name)
  local r = tas.record
  expect_ok("record.load(" .. SOURCE .. ")", r.load(SOURCE))
  if edit then
    edit(r)
  end
  expect_ok("record.save", r.save(TAS_DIR .. name .. ".tas"))
  r.stop()
  os.remove(TAS_DIR .. name .. ".tas.notes.json")
  wait_listed(name)
end

-- Loads Level 1 with `arm` (a play or translate call) armed, waits for the run to end and
-- returns to the main menu. Returns whether the run finished the level.
local function run_level(what, arm)
  local started = listen("start_level")
  local finished = listen("level_finish")
  arm()
  expect_ok("tas.level.load(1) for " .. what, tas.level.load(1))
  wait_for(what .. " to start", function() return started.fired end, 60)
  wait_for(what .. " to end", function()
    local state = tas.project.get_playback_state()
    return state ~= "playing" and state ~= "translating"
  end, 180)
  -- TAS.AutoRestart arms a finished playback again a frame later.
  tas.wait_ticks(2)
  tas.project.stop_playback()

  local exited = listen("post_exit_level")
  if tas.menu.get_current() == "dead" then
    -- The run lost every life.
    wait_screen("dead")
    expect_ok("main on the dead screen after " .. what, tas.menu.click_button("main"))
  else
    expect_ok("exit Level 1 after " .. what, tas.level.exit_to_menu())
  end
  wait_for("the exit after " .. what, function() return exited.fired end, 60)
  wait_screen("main", 60)
  return finished.fired
end

local function translate(name)
  run_level("translating " .. name, function()
    expect_ok("tas.project.translate(" .. name .. ")", tas.project.translate(name))
  end)
  local result = tas.project.get_translation_result()
  check(result and result.ok and result.project,
    "translating " .. name .. ": " .. tostring(result and result.message))
  wait_listed(result.project)
  return result.project
end

local function keys(list)
  return list and table.concat(list, "+") or "-"
end

local generated = {}

-- Plays `name` against `reference` bit for bit; returns the report and whether it finished.
-- With `may_stop_early`, the run may end before the reference as long as every frame it has
-- matches: a level script ends with its level context at a game over, while a record keeps
-- playing on the dead screen.
local function play_exact(name, reference, what, may_stop_early)
  local finished = run_level(what, function()
    expect_ok("tas.project.play(" .. name .. ")",
      tas.project.play(name, { reference = reference, exact = true }))
  end)
  local report = tas.validation.get_report()
  generated[#generated + 1] = { dump = report.dump_path }
  check(report.has_reference and report.compared_frames > 1000, string.format(
    "%s: only %d frames compared against %s", what, report.compared_frames, reference))
  local stoppedEarly = may_stop_early and report.live_frames < report.reference_frames
    and report.divergent_frames == report.reference_frames - report.live_frames
  check(report.identical or stoppedEarly, string.format(
    "%s: differs from frame %s (%d frames differ, %.9g position, %.9g velocity)",
    what, tostring(report.first_divergence_frame), report.divergent_frames,
    report.max_position_error, report.max_velocity_error))
  return report, finished
end

-- One full loop. Returns the generated script's name, the record's reference dump and
-- whether the record and the script both finished the level.
local function loop(name)
  local recordFinished = run_level("playing " .. name, function()
    expect_ok("tas.project.play(" .. name .. ")", tas.project.play(name, { validate = true }))
  end)
  local reference = tas.validation.get_report()
  check(reference.dump_path, name .. ": the playback wrote no validation dump")
  generated[#generated + 1] = { dump = reference.dump_path }
  -- A record must replay itself exactly on a later level load before a script can.
  play_exact(name, reference.dump_path, "replaying " .. name)

  local script = translate(name)
  generated[#generated + 1] = { project = script }
  local back = translate(script)
  generated[#generated + 1] = { project = back }

  -- A translation ends with the run, so frames the record has past it do not count.
  local same = tas.record.compare(name, back, { prefix = true })
  check(same, "tas.record.compare(" .. name .. ", " .. back .. ") failed")
  check(same.identical, string.format(
    "%s -> %s -> %s: frame %s differs (%d vs %d frames; keys %s vs %s; %s vs %s ms)",
    name, script, back, tostring(same.first_difference), same.frames_a, same.frames_b,
    keys(same.keys_a), keys(same.keys_b), tostring(same.frame_time_a), tostring(same.frame_time_b)))

  local report, scriptFinished = play_exact(script, reference.dump_path, script .. " playing like " .. name, true)

  tas.log("Loop Smoke %s: %d of %d frames round-trip unchanged, %d frames of physics identical, %s",
    name, same.compared_frames, same.frames_a, report.compared_frames,
    recordFinished and scriptFinished and "both finished the level" or "the run did not finish")
  return script, reference.dump_path, recordFinished and scriptFinished, same.compared_frames
end

local function script_text(name)
  local file = io.open(TAS_DIR .. name .. "/main.lua", "r")
  check(file, "cannot read the script of " .. name)
  local text = file:read("a")
  file:close()
  return text
end

-- Output names of a clean run, removed up front in case a failed run left them behind.
local DEFAULT_OUTPUTS = {
  "LoopSmoke_Constant", "LoopSmoke_Constant_Script", "LoopSmoke_Constant_Script_Record",
  "LoopSmoke_Variable", "LoopSmoke_Variable_Script", "LoopSmoke_Variable_Script_Record",
}

local function cleanup()
  for _, name in ipairs(DEFAULT_OUTPUTS) do
    remove_project(name)
  end
  for _, item in ipairs(generated) do
    if item.project then
      remove_project(item.project)
    elseif item.dump then
      os.remove(item.dump)
      os.remove(item.dump .. ".report.txt")
    end
  end
end

return function()
  wait_screen("main", 60)
  cleanup()
  local ok, err = pcall(function()
    -- Level Smoke runs before this module and leaves a level early, which is what used to
    -- make the next replay differ (see docs/IVP-Determinism-Analysis.md).
    make_record("LoopSmoke_Constant")
    local constant, constantDump, finished, roundTrip = loop("LoopSmoke_Constant")
    check(finished, "the SR_01 record and its script should both finish Level 1")
    check(roundTrip > 19000, "the round trip should cover the whole run, not " .. roundTrip .. " frames")
    check(not script_text(constant):find("set_frame_time", 1, true),
      "a constant-time record needs no tas.set_frame_time")

    -- Frame times change during the load and before the run starts. The run may no longer
    -- finish; record and script must still play alike.
    make_record("LoopSmoke_Variable", function(r)
      for frame = 20, 40 do
        expect_ok("set_frame_delta_time", r.set_frame_delta_time(frame, 5.0))
      end
      for frame = 200, 220 do
        expect_ok("set_frame_delta_time", r.set_frame_delta_time(frame, 3.5))
      end
    end)
    local variable = loop("LoopSmoke_Variable")
    check(script_text(variable):find("tas.set_frame_time(5)", 1, true),
      "the variable-time script should ask for its frame times")

    -- The changed frame times reach the game: the run no longer plays like the original.
    run_level("playing " .. variable .. " against the constant run", function()
      expect_ok("tas.project.play(" .. variable .. ")",
        tas.project.play(variable, { reference = constantDump, exact = true }))
    end)
    local changed = tas.validation.get_report()
    generated[#generated + 1] = { dump = changed.dump_path }
    check(changed.has_reference and not changed.identical,
      "the variable frame times should change the run, but it played like the constant one")
  end)
  -- A failed run keeps its records, scripts and dumps for inspection.
  if not ok then
    error(err, 0)
  end
  cleanup()
end
