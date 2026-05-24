return function()
  local saveName = "lua_runtime_smoke"
  tas.savestate.del(saveName)
  local saveErr = tas.savestate.save(saveName, "Lua runtime smoke")
  local saveList = tas.savestate.list()
  if type(saveList) ~= "table" then
    error("tas.savestate.list failed")
  end
  local directory = tas.savestate.get_directory()
  if type(directory) ~= "string" then
    error("tas.savestate.get_directory failed")
  end
  if directory:find("BallanceTAS", 1, true) then
    error("savestate directory should not create BallanceTAS folder: " .. directory)
  end
  if saveErr ~= nil then
    if not string.find(tostring(saveErr), "Ball entity not available", 1, true) then
      error("tas.savestate.save failed: " .. tostring(saveErr))
    end
  else
    if not tas.savestate.exists(saveName) then
      error("tas.savestate.exists failed")
    end
    local saveInfo = tas.savestate.get_info(saveName)
    if type(saveInfo) ~= "table" or saveInfo.name ~= saveName or type(saveInfo.position) ~= "table" then
      error("tas.savestate.get_info failed")
    end
    local delErr = tas.savestate.del(saveName)
    if delErr ~= nil then
      error("tas.savestate.del failed: " .. tostring(delErr))
    end
  end
end
