-- Keeps docs/tas.d.lua and the running API in sync: everything documented must exist,
-- and every function or class member the runtime exposes must be documented.
local documentedList = require("api_surface_list")

local documented = {}
for _, name in ipairs(documentedList) do
  documented[name] = true
end

local function resolve(path)
  local value = _G
  for part in path:gmatch("[^%.]+") do
    if type(value) ~= "table" then
      return nil
    end
    value = value[part]
  end
  return value
end

local function isCallable(value)
  if type(value) == "function" then
    return true
  end
  local mt = getmetatable(value)
  return type(mt) == "table" and mt.__call ~= nil
end

-- Runtime tas.* functions. Capitalised keys are class aliases (tas.VxVector, tas.record.MarkerType).
local function collectRuntimeFunctions(tbl, prefix, out, seen)
  if seen[tbl] then
    return
  end
  seen[tbl] = true
  for key, value in pairs(tbl) do
    if type(key) == "string" and not key:find("^__") and not key:find("^%u") then
      local name = prefix .. "." .. key
      if type(value) == "function" then
        out[#out + 1] = name
      elseif type(value) == "table" then
        collectRuntimeFunctions(value, name, out, seen)
      end
    end
  end
end

local registry = debug.getregistry()

local function classChain(className)
  local chain = {}
  local mt = registry["BallanceTAS." .. className]
  while type(mt) == "table" do
    chain[#chain + 1] = mt
    mt = rawget(mt, "__tas_base_metatable")
  end
  return chain
end

local function shortName(mt)
  local name = rawget(mt, "__name")
  return type(name) == "string" and name:gsub("^BallanceTAS%.", "") or nil
end

local function chainHas(className, field, member)
  for _, mt in ipairs(classChain(className)) do
    local members = rawget(mt, field)
    if type(members) == "table" and members[member] ~= nil then
      return true
    end
  end
  return false
end

local function documentedOnChain(className, separator, member)
  for _, mt in ipairs(classChain(className)) do
    local name = shortName(mt)
    if name and documented[name .. separator .. member] then
      return true
    end
  end
  return false
end

return function()
  local problems = {}

  -- 1. Documented names exist.
  for _, name in ipairs(documentedList) do
    if name:find("^tas%.") then
      if not isCallable(resolve(name)) then
        problems[#problems + 1] = "documented but missing: " .. name
      end
    else
      local className, separator, member = name:match("^([%w_]+)([:%.])([%w_]+)$")
      if not className then
        problems[#problems + 1] = "malformed entry: " .. name
      else
        local field = separator == ":" and "__tas_methods" or "__tas_getters"
        if not chainHas(className, field, member) then
          problems[#problems + 1] = "documented but missing: " .. name
        end
      end
    end
  end

  -- 2. Runtime functions are documented.
  local runtime = {}
  collectRuntimeFunctions(tas, "tas", runtime, {})
  for _, name in ipairs(runtime) do
    if not documented[name] then
      problems[#problems + 1] = "undocumented: " .. name
    end
  end

  -- 3. Class members are documented (on the class or one of its bases).
  for key, mt in pairs(registry) do
    if type(key) == "string" and key:find("^BallanceTAS%.") and type(mt) == "table" and rawget(mt, "__tas_methods") then
      local className = key:gsub("^BallanceTAS%.", "")
      for field, separator in pairs({ __tas_methods = ":", __tas_getters = "." }) do
        for member in pairs(rawget(mt, field) or {}) do
          if type(member) == "string" and not member:find("^__")
              and not documentedOnChain(className, separator, member) then
            problems[#problems + 1] = "undocumented: " .. className .. separator .. member
          end
        end
      end
    end
  end

  if #problems > 0 then
    table.sort(problems)
    error("API surface does not match docs/tas.d.lua:\n  " .. table.concat(problems, "\n  "), 0)
  end
  tas.log("API Surface checked %d documented names, %d runtime functions", #documentedList, #runtime)
end
