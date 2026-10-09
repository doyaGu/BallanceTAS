local util = require("smoke_util")
local check = util.check

return function()
  check(tas.context.get_name() == "global", "tas.context.get_name: " .. tostring(tas.context.get_name()))
  check(math.type(tas.context.get_type()) == "integer", "tas.context.get_type")

  _G.__lua_runtime_smoke_context_value = "global"
  check(_G.__lua_runtime_smoke_context_value == "global", "global VM write/read")
end
