-- Helpers shared by the smoke modules.
local util = {}

-- Fails the module with `message` unless `condition` holds.
function util.check(condition, message)
  if not condition then
    error(message, 2)
  end
  return condition
end

-- For API calls that return true, or false and a reason.
function util.expect_ok(what, ok, err)
  if not ok then
    error(what .. " failed: " .. tostring(err), 2)
  end
end

function util.near(a, b, epsilon)
  return math.abs(a - b) < (epsilon or 1e-4)
end

-- Waits until predicate() is true. Bounded by wall-clock seconds: level loads and
-- intros run on game time, which follows the real clock outside TAS playback.
function util.wait_for(what, predicate, seconds)
  local deadline = os.time() + (seconds or 30)
  while not predicate() do
    if os.time() > deadline then
      error("timed out waiting for " .. what, 2)
    end
    tas.wait_ticks(1)
  end
end

-- Arms a one-shot listener; call it before the action that fires the event.
-- The returned state gets `fired` and the event table in `event`.
function util.listen(name)
  local state = { fired = false }
  tas.once(name, function(event)
    state.fired = true
    state.event = event
  end)
  return state
end

-- Waits for a menu screen, plus the frames menus take to switch their buttons on.
function util.wait_screen(name, seconds)
  util.wait_for("the '" .. tostring(name) .. "' screen", function()
    return tas.menu.get_current() == name
  end, seconds)
  tas.wait_ticks(2)
end

-- Presses a button of the current screen; optionally waits for the next screen.
function util.click(button, screen)
  util.expect_ok("click_button(" .. button .. ")", tas.menu.click_button(button))
  if screen then
    util.wait_screen(screen)
  end
end

-- Waits until no playback or translation runs; an armed TAS counts as ended.
function util.wait_playback_end(what, seconds)
  util.wait_for(what .. " to end", function()
    local state = tas.project.get_playback_state()
    return state ~= "playing" and state ~= "translating"
  end, seconds or 180)
end

-- Stops the TAS (TAS.AutoRestart arms a finished playback again a frame later) and leaves
-- the level for the main menu, through the dead screen when the run lost every life.
function util.leave_level(what)
  tas.wait_ticks(2)
  tas.project.stop_playback()
  local exited = util.listen("post_exit_level")
  if tas.menu.get_current() == "dead" then
    util.wait_screen("dead")
    util.expect_ok("main on the dead screen after " .. what, tas.menu.click_button("main"))
  else
    util.expect_ok("exit the level after " .. what, tas.level.exit_to_menu())
  end
  util.wait_for("the exit after " .. what, function() return exited.fired end, 60)
  util.wait_screen("main", 60)
end

return util
