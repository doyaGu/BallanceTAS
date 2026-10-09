local util = require("smoke_util")
local check, expect_ok = util.check, util.expect_ok

return function()
  local saveName = "lua_runtime_smoke"
  tas.savestate.remove(saveName)

  check(type(tas.savestate.list()) == "table", "tas.savestate.list")
  local directory = tas.savestate.get_directory()
  check(type(directory) == "string", "tas.savestate.get_directory")
  check(not directory:find("BallanceTAS", 1, true), "savestate directory should not create a BallanceTAS folder: " .. directory)
  check(not pcall(tas.savestate.save, ""), "tas.savestate.save should refuse an empty name")

  local saved, reason = tas.savestate.save(saveName, "Lua runtime smoke")
  if not saved then
    -- At the menu there is no ball to save; the reason says so instead of raising.
    check(type(reason) == "string" and reason:find("Ball entity not available", 1, true),
      "tas.savestate.save failed: " .. tostring(reason))
    check(tas.savestate.load(saveName) == false, "loading a missing savestate should report a failure")
    return
  end

  check(tas.savestate.exists(saveName), "tas.savestate.exists")
  local info = tas.savestate.get_info(saveName)
  check(type(info) == "table" and info.name == saveName and type(info.position.x) == "number", "tas.savestate.get_info")
  expect_ok("tas.savestate.remove", tas.savestate.remove(saveName))
  check(tas.savestate.get_info(saveName) == nil, "get_info of a removed savestate")
end
