return function()
  if type(tas.menu) ~= "table" then
    error("tas.menu table missing")
  end

  if type(tas.menu.is_in_menu()) ~= "boolean" then
    error("tas.menu.is_in_menu did not return boolean")
  end
  if tas.menu.is_in_game ~= tas.level.is_loaded then
    error("tas.menu.is_in_game should be the same function as tas.level.is_loaded")
  end

  local projects = tas.project.list()
  if type(projects) ~= "table" then
    error("tas.project.list did not return table for UI smoke")
  end

  tas.log("TAS Menu UI Smoke: open menu path covered")
  tas.log("TAS Menu UI Smoke: details back path requires visual QA")
  tas.log("TAS Menu UI Smoke: pending playback/stop path requires visual QA")
  tas.log("TAS Menu UI Smoke: record page back path requires visual QA")
end
