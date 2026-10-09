local util = require("smoke_util")
local check, expect_ok = util.check, util.expect_ok

return function()
  util.wait_screen("main", 10)

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
  util.wait_screen("main")

  -- Keys reach menus outside playback: Esc on the level list goes back to the main menu.
  expect_ok("click_button(start)", tas.menu.click_button("start"))
  util.wait_screen("start")
  expect_ok("press_escape", tas.menu.press_escape())
  util.wait_screen("main")
  tas.log("Menu Smoke keys and buttons PASS checks")
end
