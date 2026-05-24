return function()
  local manifest = tas.get_manifest()
  if type(manifest) ~= "table" or manifest.name ~= "LuaRuntimeSmoke" then
    error("tas.get_manifest failed")
  end

  local projects = tas.project.list()
  if type(projects) ~= "table" or #projects < 1 then
    error("tas.project.list failed")
  end
  local current_project = tas.project.get_current()
  if current_project ~= nil and type(current_project) ~= "table" then
    error("tas.project.get_current failed")
  end
  local found_project = tas.project.find("LuaRuntimeSmoke")
  if type(found_project) ~= "table" or found_project.scope ~= "global" then
    error("tas.project.find failed")
  end
  if type(tas.project.is_loaded()) ~= "boolean" then
    error("tas.project.is_loaded failed")
  end

  if type(tas.get_tick()) ~= "number" then
    error("tas.get_tick failed")
  end
  if type(tas.level.get_current()) ~= "string" then
    error("tas.level.get_current failed")
  end
  if type(tas.level.get_current_number()) ~= "number" or type(tas.level.get_sector()) ~= "number" then
    error("tas.level numeric query failed")
  end
  if type(tas.level.is_loaded()) ~= "boolean" or type(tas.level.is_paused()) ~= "boolean" then
    error("tas.level state query failed")
  end
  if tas.level.is_completed() ~= false or type(tas.level.is_at_checkpoint(0)) ~= "boolean" then
    error("tas.level checkpoint query failed")
  end
  if tas.menu.is_in_menu() ~= true or tas.menu.is_in_game() ~= false then
    error("tas.menu state query failed")
  end
  if pcall(tas.menu.get_current) then
    error("tas.menu.get_current should report the unimplemented API cleanly")
  end

  if type(tas.record.is_playing()) ~= "boolean" or type(tas.record.is_paused()) ~= "boolean" then
    error("tas.record state query failed")
  end
  if type(tas.record.get_current_frame()) ~= "number" or type(tas.record.get_total_frames()) ~= "number" then
    error("tas.record frame query failed")
  end
end
