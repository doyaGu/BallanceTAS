package.path = table.concat({
  package.path,
  "./?.lua",
  "../ModLoader/TAS/LuaRuntimeSmoke/?.lua",
  "ModLoader/TAS/LuaRuntimeSmoke/?.lua"
}, ";")

local modules = {
  { name = "api_surface_smoke", label = "API Surface Smoke" },
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
  { name = "validation_smoke", label = "Validation Smoke" },
  { name = "api_extras_smoke", label = "API Extras Smoke" },
  { name = "menu_smoke", label = "Menu Smoke" },
  { name = "level_smoke", label = "Level Smoke" },
  { name = "loop_smoke", label = "Loop Smoke" },
  { name = "game_flow_smoke", label = "Game Flow Smoke" },
}

-- Optional smoke_only.txt (one module name per line) runs just those modules, for quick iteration.
local function read_only_list()
  for _, path in ipairs({ "smoke_only.txt", "../ModLoader/TAS/LuaRuntimeSmoke/smoke_only.txt" }) do
    local file = io.open(path, "r")
    if file then
      local only = {}
      for line in file:lines() do
        local name = line:match("^%s*(.-)%s*$")
        if name ~= "" then
          only[name] = true
        end
      end
      file:close()
      return only
    end
  end
  return nil
end

local only = read_only_list()
if only then
  local selected = {}
  for _, module in ipairs(modules) do
    if only[module.name] then
      selected[#selected + 1] = module
    end
  end
  modules = selected
end

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

local function run_all()
  tas.log("BallanceTAS LuaRuntime Smoke START tick=" .. tostring(tas.get_tick()))
  for _, module in ipairs(modules) do
    run_module(module)
  end
  tas.log("BallanceTAS LuaRuntime Smoke PASS")
  tas.log("BallanceTAS LuaRuntime Full Smoke PASS")
end

function main()
  local ok, err = xpcall(run_all, debug.traceback)
  if ok then
    if tas.harness and tas.harness.is_active() then
      tas.harness.pass("BallanceTAS LuaRuntime Full Smoke PASS")
    end
    return
  end

  tas.log("BallanceTAS LuaRuntime Full Smoke FAIL: " .. tostring(err))
  if tas.harness and tas.harness.is_active() then
    tas.harness.fail(tostring(err))
    return
  end
  error(err)
end
