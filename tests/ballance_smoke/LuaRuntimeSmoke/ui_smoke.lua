return function()
  if type(tas.menu) ~= "table" then
    error("tas.menu table missing")
  end

  if type(tas.menu.is_in_menu()) ~= "boolean" then
    error("tas.menu.is_in_menu did not return boolean")
  end
  if type(tas.menu.is_in_game()) ~= "boolean" then
    error("tas.menu.is_in_game did not return boolean")
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
