-- Drives the real level flow through tas.level / tas.menu:
-- main menu -> load Level 1 -> restart -> exit -> load Level 2 -> exit.

-- Level intros run on game time, which follows the real clock outside TAS
-- playback, so time out on wall-clock seconds rather than ticks.
local TIMEOUT_SECONDS = 30

local function wait_for(description, predicate)
  local deadline = os.time() + TIMEOUT_SECONDS
  tas.wait_until(function()
    return predicate() or os.time() > deadline
  end)
  if not predicate() then
    error("timed out waiting for " .. description)
  end
end

-- Arm a one-shot event listener before triggering the action that fires it.
local function listen(event)
  local state = { fired = false }
  tas.async.spawn(function()
    tas.wait_event(event)
    state.fired = true
  end)
  return state
end

local function expect_ok(description, ok, err)
  if not ok then
    error(description .. " failed: " .. tostring(err))
  end
end

return function()
  if tas.level.is_loaded() then
    error("level smoke must start from a menu")
  end

  wait_for("the main menu", function() return tas.menu.get_current() ~= nil end)
  local menu = tas.menu.get_current()
  tas.log("Level Smoke menu=" .. tostring(menu))
  if menu ~= "main" and menu ~= "start" then
    error("expected the main menu or level selection, got " .. tostring(menu))
  end
  if not tas.menu.is_at(menu) or not tas.menu.is_at("Menu_" .. menu:gsub("^%l", string.upper)) then
    error("tas.menu.is_at does not match tas.menu.get_current")
  end

  local levels = tas.menu.get_available_levels()
  if #levels ~= 13 or levels[1] ~= "Level_01" or levels[13] ~= "Level_13" then
    error("tas.menu.get_available_levels returned an unexpected list")
  end

  local ok, err = pcall(tas.level.load, 14)
  if ok then
    error("tas.level.load(14) should reject non-built-in levels")
  end

  -- Load Level 1 from the menu.
  local started = listen("start_level")
  local controllable = listen("ball_nav_active")
  expect_ok("tas.level.load(1)", tas.level.load("Level_01"))
  wait_for("Level 1 to start", function() return started.fired end)
  wait_for("ball control in Level 1", function() return controllable.fired end)
  if tas.level.get_current_number() ~= 1 then
    error("expected Level 1, got " .. tostring(tas.level.get_current_number()))
  end
  if tas.level.is_completed() then
    error("tas.level.is_completed should be false right after loading")
  end
  local velocity = tas.get_ball_velocity()
  local angular = tas.get_ball_angular_velocity()
  local body = tas.get_physics_object(tas.get_ball())
  if velocity == nil or angular == nil or body == nil then
    error("ball velocity / physics object should be readable once the ball is controllable")
  end
  local bodyVelocity, bodyAngular = body:get_velocity()
  if bodyVelocity.x ~= velocity.x or bodyVelocity.y ~= velocity.y or bodyVelocity.z ~= velocity.z
      or bodyAngular.x ~= angular.x or bodyAngular.y ~= angular.y or bodyAngular.z ~= angular.z then
    error("tas.get_ball_velocity disagrees with PhysicsObject:get_velocity")
  end
  local loadOk, loadErr = tas.level.load(2)
  if loadOk or not tostring(loadErr):find("already in a level", 1, true) then
    error("tas.level.load inside a level should fail, got " .. tostring(loadOk) .. " " .. tostring(loadErr))
  end
  if math.type(tas.get_life_count()) ~= "integer" or tas.get_life_count() < 1 then
    error("tas.get_life_count should report the lives of a running level")
  end
  if math.type(tas.get_points()) ~= "integer" or type(tas.get_sr_score()) ~= "number"
      or math.type(tas.get_hs_score()) ~= "integer" then
    error("tas.get_points / get_sr_score / get_hs_score returned the wrong types")
  end
  local ball = tas.get_ball()
  local moved = pcall(function() ball:set_position(ball:get_position()) end)
  if moved then
    error("moving the physicalized ball through CK3dEntity should be refused")
  end
  tas.log("Level Smoke loaded Level 1")

  -- Restart the level; tas.once receives the event table.
  local reset = listen("post_reset_level")
  local resetEvent = nil
  tas.once("post_reset_level", function(event) resetEvent = event end)
  expect_ok("tas.level.restart()", tas.level.restart())
  wait_for("the level restart", function() return reset.fired end)
  if type(resetEvent) ~= "table" or resetEvent.name ~= "post_reset_level" or type(resetEvent.tick) ~= "number" then
    error("tas.once should deliver the post_reset_level event table")
  end
  tas.log("Level Smoke restarted Level 1")

  -- The game only offers "next level" on the end screen.
  local nextOk, nextErr = tas.level.next()
  if nextOk or not tostring(nextErr):find("after finishing", 1, true) then
    error("tas.level.next mid-level should fail, got " .. tostring(nextOk) .. " " .. tostring(nextErr))
  end

  -- Back to the menu.
  local exited = listen("post_exit_level")
  expect_ok("tas.level.exit_to_menu()", tas.level.exit_to_menu())
  wait_for("the level to exit", function() return exited.fired end)
  wait_for("a menu to appear", function() return tas.menu.get_current() ~= nil end)
  if tas.level.is_loaded() then
    error("still in a level after tas.level.exit_to_menu()")
  end
  tas.log("Level Smoke back at menu=" .. tostring(tas.menu.get_current()))

  -- A second load from wherever exit_to_menu() left us.
  local started2 = listen("start_level")
  local controllable2 = listen("ball_nav_active")
  expect_ok("tas.level.load(2)", tas.menu.select_level(2))
  wait_for("Level 2 to start", function() return started2.fired end)
  wait_for("ball control in Level 2", function() return controllable2.fired end)
  if tas.level.get_current_number() ~= 2 then
    error("expected Level 2, got " .. tostring(tas.level.get_current_number()))
  end
  tas.log("Level Smoke loaded Level 2")

  local exited2 = listen("post_exit_level")
  expect_ok("tas.menu.go_to_main()", tas.menu.go_to_main())
  wait_for("Level 2 to exit", function() return exited2.fired end)
  wait_for("a menu to appear", function() return tas.menu.get_current() ~= nil end)
  tas.log("Level Smoke finished at menu=" .. tostring(tas.menu.get_current()))
end
