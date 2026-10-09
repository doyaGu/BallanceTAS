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

return util
