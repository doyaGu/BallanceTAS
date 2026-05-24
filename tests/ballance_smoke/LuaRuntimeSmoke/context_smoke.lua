return function()
  if tas.context.get_name() ~= "global" then
    error("tas.context.get_name failed: " .. tostring(tas.context.get_name()))
  end
  if type(tas.context.get_type()) ~= "number" then
    error("tas.context.get_type failed")
  end

  _G.__lua_runtime_smoke_context_value = "global"
  if _G.__lua_runtime_smoke_context_value ~= "global" then
    error("global VM write/read failed")
  end
end
