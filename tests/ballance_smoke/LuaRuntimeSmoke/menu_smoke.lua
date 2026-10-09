local function check(condition, message)
  if not condition then
    error(message, 2)
  end
end

local function expect_ok(what, ok, err)
  if not ok then
    error(what .. " failed: " .. tostring(err), 2)
  end
end

local function wait_for_screen(name, ticks)
  for _ = 1, ticks do
    if tas.menu.get_current() == name then
      tas.wait_ticks(2)
      return true
    end
    tas.wait_ticks(1)
  end
  return false
end

return function()
  check(wait_for_screen("main", 600), "menu smoke expects to start at the main menu")

  local ok, err = tas.menu.click_button("no_such_button")
  check(not ok and tostring(err):find("not a button", 1, true), "unknown buttons should be rejected")

  -- Walk a few screens through their own buttons.
  expect_ok("navigate_to(options_sound)", tas.menu.navigate_to("options_sound"))
  check(tas.menu.get_current() == "options_sound", "should be on the sound options")
  expect_ok("navigate_to(main) from options_sound", tas.menu.navigate_to("main"))
  check(tas.menu.get_current() == "main", "should be back at the main menu")
  tas.log("Menu Smoke walked main -> options -> sound -> main")

  expect_ok("navigate_to(credits)", tas.menu.navigate_to("credits"))
  expect_ok("go_back from credits", tas.menu.go_back())
  check(wait_for_screen("main", 132), "credits back should return to the main menu")

  -- Keys reach menus outside playback: Esc on the level list goes back to the main menu.
  expect_ok("click_button(start)", tas.menu.click_button("start"))
  check(wait_for_screen("start", 132), "start should open the level list")
  expect_ok("press_escape", tas.menu.press_escape())
  check(wait_for_screen("main", 132), "Esc on the level list should return to the main menu")
  tas.log("Menu Smoke keys and buttons PASS checks")
end
