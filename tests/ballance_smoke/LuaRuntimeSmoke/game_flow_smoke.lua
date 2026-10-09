-- Drives the in-level menus (pause, yes/no, dead, score, end) through their own buttons.
local TIMEOUT_SECONDS = 150

local util = require("smoke_util")
local check, expect_ok, wait_for, listen = util.check, util.expect_ok, util.wait_for, util.listen
local wait_screen, click = util.wait_screen, util.click

local function load_level(level)
  local controllable = listen("ball_nav_active")
  expect_ok("tas.level.load(" .. level .. ")", tas.level.load(level))
  wait_for("ball control in level " .. level, function() return controllable.fired end, 60)
end

local function open_pause()
  expect_ok("press_escape in game", tas.menu.press_escape())
  wait_screen("pause")
end

local function pause_menu()
  load_level(1)

  open_pause()
  click("options", "options")
  expect_ok("go_back from options", tas.menu.go_back())
  wait_screen("pause")
  click("highscore", "highscore")
  expect_ok("go_back from highscore", tas.menu.go_back())
  wait_screen("pause")

  click("restart", "yesno")
  click("no", "pause")

  click("back")
  wait_for("the level to unpause", function() return tas.menu.get_current() == nil and not tas.is_paused() end)
  tas.log("Game Flow pause menu: options, highscore, no, back")

  local reset = listen("post_reset_level")
  open_pause()
  click("restart", "yesno")
  local controllable = listen("ball_nav_active")
  click("yes")
  wait_for("the restart from the pause menu", function() return reset.fired end)
  tas.log("Game Flow pause menu: restart confirmed")
  -- The game ignores Esc until the restarted level hands control back.
  wait_for("ball control after the restart", function() return controllable.fired end, 60)

  local exited = listen("post_exit_level")
  open_pause()
  click("exit", "yesno")
  click("yes")
  wait_for("the exit from the pause menu", function() return exited.fired end)
  wait_screen("main", 30)
  tas.log("Game Flow pause menu: exit confirmed")
end

-- Throws the ball off the level until the game is over.
local function lose_all_lives()
  local dead = listen("dead")
  local deadline = os.time() + 120
  while not dead.fired do
    check(os.time() < deadline, "the game did not end after losing every life")
    local lives = tas.get_life_count()
    local ball = tas.get_ball()
    local body = ball and tas.get_physics_object(ball)
    local position = tas.get_ball_position()
    if body and position then
      tas.log("Game Flow throwing the ball off: lives=%d y=%.2f", lives, position.y)
      -- A gentle push sideways (Level 1 runs along z): the ball drops just beside the
      -- track onto the level's depth triggers. Thrown far away it would miss them.
      body:set_linear_velocity(VxVector(25, 0, 0))
      -- Wait until the life is gone (or the game is over), then for the next ball.
      wait_for("a life to be lost", function() return dead.fired or tas.get_life_count() < lives end, 30)
      if not dead.fired then
        wait_for("the ball to respawn", function()
          local p = tas.get_ball_position()
          return dead.fired or (p ~= nil and tas.get_physics_object(tas.get_ball()) ~= nil and p.y > -5)
        end, 30)
        tas.wait_ticks(132)
      end
    else
      tas.wait_ticks(1)
    end
  end
  wait_screen("dead", 30)
end

local function dead_menu()
  load_level(1)
  lose_all_lives()
  local reset = listen("post_reset_level")
  local controllable = listen("ball_nav_active")
  click("restart")
  wait_for("the restart from the dead screen", function() return reset.fired end)
  tas.log("Game Flow dead screen: restart")

  wait_for("ball control after the restart", function() return controllable.fired end, 60)
  lose_all_lives()
  local exited = listen("post_exit_level")
  click("main")
  wait_for("the exit from the dead screen", function() return exited.fired end)
  wait_screen("main", 30)
  tas.log("Game Flow dead screen: main menu")
end

-- The Level 1 run whose replay was verified frame-identical across level re-entries.
local FINISH_PROJECT = "SR_01_1.16.726_Script"

-- Finish Level 1 with that run, then work through the end screens.
local function end_menu()
  check(tas.project.find(FINISH_PROJECT) ~= nil, "the game flow smoke needs the " .. FINISH_PROJECT .. " project")
  expect_ok("tas.project.play(" .. FINISH_PROJECT .. ")", tas.project.play(FINISH_PROJECT))
  local record = FINISH_PROJECT

  local finished = listen("level_finish")
  local lost = listen("ball_off")
  expect_ok("tas.level.load(1) for the run", tas.level.load(1))
  wait_for("the run to finish Level 1", function()
    -- A verified run never loses a life; fail now instead of waiting out the timeout.
    check(not lost.fired, "the replay lost a life at tick " .. tostring(lost.event and lost.event.tick))
    return finished.fired
  end, 150)
  check(tas.level.is_completed(), "the level should be completed")
  tas.log("Game Flow finished Level 1 with %s", record)

  -- Score, optional highscore entry and table, then the end screen.
  local deadline = os.time() + 60
  while tas.menu.get_current() ~= "end" do
    check(os.time() < deadline, "the end screen did not appear")
    local screen = tas.menu.get_current()
    if screen == "score" then
      tas.menu.press_enter()
    elseif screen == "highscore_entry" then
      tas.wait_ticks(2)
      tas.menu.click_button("ok")
    elseif screen == "highscore" then
      tas.wait_ticks(2)
      tas.menu.click_button("next")
    end
    tas.wait_ticks(5)
  end
  wait_screen("end")

  click("options", "options")
  expect_ok("go_back from options on the end screen", tas.menu.go_back())
  wait_screen("end")

  local started = listen("start_level")
  click("next")
  wait_for("Level 2 to start", function() return started.fired end, 60)
  check(tas.level.get_number() == 2, "next should load Level 2")
  tas.log("Game Flow end screen: options, next level")

  local exited = listen("post_exit_level")
  expect_ok("exit Level 2", tas.level.exit_to_menu())
  wait_for("the exit from Level 2", function() return exited.fired end)
  wait_screen("main", 30)
end

return function()
  local started = os.time()
  wait_screen("main", 60)
  -- The finishing replay runs last, after several level loads, on purpose: a level load
  -- right after another one must still take as many frames as the recording expects.
  pause_menu()
  dead_menu()
  end_menu()
  check(os.time() - started < TIMEOUT_SECONDS * 2, "game flow smoke took unusually long")
end
