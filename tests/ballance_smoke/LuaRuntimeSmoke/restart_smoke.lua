local util = require("smoke_util")
local check, expect_ok, wait_for, listen, wait_screen =
  util.check, util.expect_ok, util.wait_for, util.listen, util.wait_screen

-- An in-level restart with a TAS armed loads the level again, so the TAS starts the way it
-- does at a level load and plays exactly as it does there.
local PROJECT = "SR_01_1.16.726"

local wait_playback_end, leave_level = util.wait_playback_end, util.leave_level

local function playback_state()
  return tas.project.get_playback_state()
end

-- Restarts Level 1 and waits until the restart has loaded it again.
local function restart_reloads(what)
  local reset, exited, loaded = listen("pre_reset_level"), listen("post_exit_level"), listen("pre_load_level")
  expect_ok("tas.level.restart() " .. what, tas.level.restart())
  wait_for("the restart " .. what .. " to load Level 1 again", function() return loaded.fired end, 60)
  check(reset.fired and exited.fired, "the restart " .. what .. " should restart, leave and load Level 1")
  wait_for("the playback after the restart " .. what, function() return playback_state() == "playing" end, 30)
end

return function()
  wait_screen("main", 60)
  local dumps = {}
  -- Reference: the record from a level load.
  local started = listen("start_level")
  expect_ok("tas.project.play(" .. PROJECT .. ")", tas.project.play(PROJECT, { validate = true }))
  expect_ok("tas.level.load(1)", tas.level.load(1))
  wait_for("the reference playback to start", function() return started.fired end, 60)
  wait_playback_end("the reference playback")
  local reference = tas.validation.get_report()
  check(reference.dump_path, "the reference playback wrote no validation dump")
  dumps[#dumps + 1] = reference.dump_path
  leave_level("the reference playback")

  -- Armed while the level is played by hand, then restarted: also clears the ball control
  -- the hand play left on.
  local nav = listen("ball_nav_active")
  expect_ok("tas.level.load(1) by hand", tas.level.load(1))
  wait_for("ball control", function() return nav.fired end, 60)
  tas.wait_ticks(30)
  expect_ok("tas.project.play(" .. PROJECT .. ") during hand play",
    tas.project.play(PROJECT, { reference = reference.dump_path, exact = true }))
  restart_reloads("during hand play")
  wait_playback_end("the playback after the restart during hand play")
  local report = tas.validation.get_report()
  dumps[#dumps + 1] = report.dump_path
  check(report.has_reference and report.compared_frames > 1000, string.format(
    "after the restart during hand play: only %d frames compared", report.compared_frames))
  check(report.identical, string.format(
    "after the restart during hand play: differs from frame %s (%d frames differ)",
    tostring(report.first_divergence_frame), report.divergent_frames))
  leave_level("the playback after the restart during hand play")

  -- Restarted during a playback: TAS.AutoRestart arms it again and it plays from the reload.
  started = listen("start_level")
  expect_ok("tas.project.play(" .. PROJECT .. ")", tas.project.play(PROJECT))
  expect_ok("tas.level.load(1)", tas.level.load(1))
  wait_for("the playback to start", function() return started.fired end, 60)
  tas.wait_ticks(600)
  check(playback_state() == "playing", "the playback should still run 600 frames in")
  local reset = listen("pre_reset_level")
  expect_ok("tas.level.restart() during playback", tas.level.restart())
  wait_for("the restart during playback", function() return reset.fired end, 30)
  tas.wait_ticks(1)
  if playback_state() == "armed" then
    local finished = listen("level_finish")
    wait_for("the playback after the restart during playback", function()
      return playback_state() == "playing"
    end, 60)
    wait_playback_end("the playback after the restart during playback")
    check(finished.fired, "the playback after the restart during playback should finish Level 1")
  else
    -- Without TAS.AutoRestart the restart ends the TAS and stays a plain restart.
    tas.log("Restart Smoke: TAS.AutoRestart is off; the restart during playback ended the TAS.")
  end
  leave_level("the restart during playback")
  for _, dump in ipairs(dumps) do
    os.remove(dump)
    os.remove(dump .. ".report.txt")
  end
end
