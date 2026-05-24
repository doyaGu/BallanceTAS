package.path = table.concat({
  package.path,
  "./?.lua",
  "../ModLoader/TAS/LuaRuntimeSmoke/?.lua",
  "ModLoader/TAS/LuaRuntimeSmoke/?.lua"
}, ";")

local modules = {
  { name = "core_smoke", label = "Core Smoke" },
  { name = "context_smoke", label = "Context Smoke" },
  { name = "shared_smoke", label = "Shared Smoke" },
  { name = "message_smoke", label = "Message Smoke" },
  { name = "input_merge_smoke", label = "Input Merge Smoke" },
  { name = "ui_smoke", label = "TAS Menu UI Smoke" },
  { name = "async_smoke", label = "Async Smoke" },
  { name = "math_smoke", label = "Math Smoke" },
  { name = "world_smoke", label = "World Smoke" },
  { name = "state_smoke", label = "State Smoke" },
  { name = "determinism_smoke", label = "Determinism Smoke" },
}

local function run_module(module)
  local ok, loaded = xpcall(require, debug.traceback, module.name)
  if not ok then
    error(module.label .. " load failed\n" .. tostring(loaded))
  end

  local runner = loaded
  if type(loaded) == "table" then
    runner = loaded.run
  end
  if type(runner) ~= "function" then
    error(module.label .. " has no run function")
  end

  local run_ok, err = xpcall(runner, debug.traceback)
  if not run_ok then
    error(module.label .. " failed\n" .. tostring(err))
  end

  tas.log(module.label .. " PASS")
end

function main()
  tas.log("BallanceTAS LuaRuntime Smoke START tick=" .. tostring(tas.get_tick()))
  for _, module in ipairs(modules) do
    run_module(module)
  end
  tas.log("BallanceTAS LuaRuntime Smoke PASS")
  tas.log("BallanceTAS LuaRuntime Full Smoke PASS")
end
