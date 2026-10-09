local util = require("smoke_util")
local check, expect_ok = util.check, util.expect_ok

local function raises(fn, pattern, what)
  local ok, err = pcall(fn)
  check(not ok, what .. " should raise")
  check(tostring(err):find(pattern, 1, true), what .. " message: " .. tostring(err))
end

return function()
  local manifest = tas.get_manifest()
  check(type(manifest) == "table" and manifest.name == "LuaRuntimeSmoke", "tas.get_manifest")

  local projects = tas.project.list()
  check(type(projects) == "table" and #projects >= 1, "tas.project.list")
  local current_project = tas.project.get_current()
  check(current_project == nil or type(current_project) == "table", "tas.project.get_current")
  local found_project = tas.project.find("LuaRuntimeSmoke")
  check(type(found_project) == "table" and found_project.scope == "global", "tas.project.find")
  check(tas.project.find("no such project") == nil, "tas.project.find of a missing project is nil")
  check(type(tas.project.is_loaded()) == "boolean", "tas.project.is_loaded")
  local loaded, why = tas.project.load("no such project")
  check(loaded == false and type(why) == "string", "tas.project.load reports a missing project")

  check(math.type(tas.get_tick()) == "integer", "tas.get_tick")

  -- At the menu: no level, so name lookups give nil.
  check(tas.level.get_name() == nil, "tas.level.get_name in a menu")
  check(math.type(tas.level.get_number()) == "integer" and math.type(tas.level.get_sector()) == "integer",
    "tas.level numeric queries")
  check(tas.level.is_loaded() == false and type(tas.level.is_paused()) == "boolean", "tas.level state queries")
  check(tas.level.is_completed() == false and type(tas.level.is_at_checkpoint(0)) == "boolean", "tas.level checkpoint queries")
  check(tas.menu.is_in_menu() == true and tas.menu.is_in_game() == false, "tas.menu state queries")
  local currentMenu = tas.menu.get_current()
  check(currentMenu == nil or type(currentMenu) == "string", "tas.menu.get_current")
  local buttons = tas.menu.get_buttons("main")
  check(type(buttons) == "table" and buttons[1] == "start", "tas.menu.get_buttons('main')")

  -- Aliases are the same function objects as their canonical names.
  check(tas.get_level == tas.level.get_number and tas.level.get_current_number == tas.level.get_number, "level number aliases")
  check(tas.is_playing == tas.level.is_loaded and tas.menu.select_level == tas.level.load, "level aliases")
  check(tas.press == tas.keyboard.press and tas.release_all_keys == tas.keyboard.release_all, "keyboard aliases")
  check(tas.async.delay == tas.wait_ticks and tas.force_gc == tas.gc.collect, "async/gc aliases")
  check(tas.savestate.del == tas.savestate.remove and tas.harness.active == tas.harness.is_active, "savestate/harness aliases")
  check(tas.record.delete_macro == tas.record.remove_macro, "record aliases")
  check(tas.VxMatrix == VxMatrix and tas.VxVector == VxVector, "class tables under tas")

  -- Internals stay out of the tas table.
  for key in pairs(tas) do
    check(type(key) ~= "string" or not key:find("^__"), "internal name leaked into tas: " .. tostring(key))
  end
  for key in pairs(tas.async) do
    check(type(key) ~= "string" or not key:find("^__"), "internal name leaked into tas.async: " .. tostring(key))
  end

  -- Caller mistakes raise "tas.<module>.<fn>: ..."; expected failures return false, reason.
  raises(function() tas.wait_ticks(1.5) end, "tas.wait_ticks: bad argument #1", "tas.wait_ticks(1.5)")
  raises(function() tas.level.load(99) end, "tas.level.load: bad argument #1", "tas.level.load(99)")
  raises(function() tas.get_object("") end, "tas.get_object: bad argument #1", "tas.get_object('')")
  raises(function() tas.skip_rendering(0) end, "tas.skip_rendering", "tas.skip_rendering(0)")
  local restarted, reason = tas.level.restart()
  check(restarted == false and reason == "not in a level", "tas.level.restart in a menu: " .. tostring(reason))
  expect_ok("gc.set_mode", tas.gc.set_mode(tas.gc.get_mode()))
  check(type(tas.gc.collect()) == "number", "tas.gc.collect returns the KB freed")

  check(type(tas.record.is_playing()) == "boolean" and type(tas.record.is_paused()) == "boolean", "tas.record state queries")
  check(math.type(tas.record.get_current_frame()) == "integer" and math.type(tas.record.get_total_frames()) == "integer",
    "tas.record frame queries")
  check(tas.record.get_frame_delta_time(123456) == nil, "tas.record.get_frame_delta_time of a missing frame is nil")
end
