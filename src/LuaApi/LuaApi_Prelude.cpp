#include "LuaApi.h"

#include <stdexcept>
#include <string>

// Script-level helpers built on the C API. They run in the script's own VM, so they can
// yield through tas.wait_* like any other script code.
static constexpr const char kPrelude[] = R"LUA(
local tas = tas

-- Event listeners --------------------------------------------------------------------

local listeners = {}
local nextListenerId = 0

local function addListener(name, callback, once)
  if type(name) ~= "string" or name == "" then
    error("event name must be a non-empty string", 3)
  end
  if type(callback) ~= "function" then
    error("callback must be a function", 3)
  end
  nextListenerId = nextListenerId + 1
  local list = listeners[name]
  if not list then
    list = {}
    listeners[name] = list
  end
  list[#list + 1] = { id = nextListenerId, callback = callback, once = once }
  return nextListenerId
end

local function dispatch(name, ...)
  local list = listeners[name]
  if not list then
    return
  end
  -- Snapshot so callbacks can add or remove listeners while we iterate.
  local snapshot = table.move(list, 1, #list, 1, {})
  local args = table.pack(...)
  for _, entry in ipairs(snapshot) do
    if entry.once then
      tas.off(entry.id)
    end
    -- Each listener runs now, in its own task, so it may tas.wait_* and resume on later ticks.
    local callback = entry.callback
    tas.async.__run_now(function()
      local ok, err = pcall(callback, table.unpack(args, 1, args.n))
      if not ok then
        tas.warn("event '%s' listener failed: %s", name, tostring(err))
      end
    end)
  end
end

function tas.on(name, callback)
  return addListener(name, callback, false)
end

function tas.once(name, callback)
  return addListener(name, callback, true)
end

function tas.off(id)
  for name, list in pairs(listeners) do
    for i, entry in ipairs(list) do
      if entry.id == id then
        table.remove(list, i)
        if #list == 0 then
          listeners[name] = nil
        end
        return true
      end
    end
  end
  return false
end

function tas.fire_event(name, ...)
  if type(name) ~= "string" or name == "" then
    error("tas.fire_event: event name must be a non-empty string", 2)
  end
  dispatch(name, ...)
end

function tas.clear_listeners(name)
  if name == nil then
    listeners = {}
  else
    listeners[name] = nil
  end
end

function tas.get_listener_count(name)
  local list = listeners[name]
  return list and #list or 0
end

function tas.has_listeners(name)
  return listeners[name] ~= nil
end

-- Flow control -----------------------------------------------------------------------

local function checkFunction(value, fn, what)
  if type(value) ~= "function" then
    error(fn .. ": " .. what .. " must be a function", 3)
  end
end

local function checkCount(value, fn, what)
  if math.type(value) ~= "integer" or value < 0 then
    error(fn .. ": " .. what .. " must be a non-negative integer", 3)
  end
end

function tas.repeat_for(task, ticks)
  checkFunction(task, "tas.repeat_for", "task")
  checkCount(ticks, "tas.repeat_for", "ticks")
  for i = 1, ticks do
    task(i)
    tas.wait_ticks(1)
  end
end

function tas.repeat_count(task, count)
  checkFunction(task, "tas.repeat_count", "task")
  checkCount(count, "tas.repeat_count", "count")
  for i = 1, count do
    task(i)
    if i < count then
      tas.wait_ticks(1)
    end
  end
end

function tas.repeat_until(task, condition)
  checkFunction(task, "tas.repeat_until", "task")
  checkFunction(condition, "tas.repeat_until", "condition")
  while true do
    task()
    if condition() then
      return
    end
    tas.wait_ticks(1)
  end
end

function tas.repeat_while(task, condition)
  checkFunction(task, "tas.repeat_while", "task")
  checkFunction(condition, "tas.repeat_while", "condition")
  while condition() do
    task()
    tas.wait_ticks(1)
  end
end

function tas.timeout(task, ticks)
  checkFunction(task, "tas.timeout", "task")
  checkCount(ticks, "tas.timeout", "ticks")
  local handle = tas.async.spawn(task)
  local waited = 0
  while not handle:is_done() and waited < ticks do
    tas.wait_ticks(1)
    waited = waited + 1
  end
  if not handle:is_done() then
    handle:cancel()
    return false, "timeout"
  end
  if handle:is_completed() then
    return true, handle:get_result()
  end
  local err = handle:get_error()
  return false, type(err) == "table" and err.message or tostring(err)
end

function tas.retry(task, attempts)
  checkFunction(task, "tas.retry", "task")
  if math.type(attempts) ~= "integer" or attempts < 1 then
    error("tas.retry: attempts must be a positive integer", 2)
  end
  local lastError
  for attempt = 1, attempts do
    local results = table.pack(pcall(task, attempt))
    if results[1] then
      return table.unpack(results, 2, results.n)
    end
    lastError = results[2]
  end
  error(lastError, 0)
end

-- Shared data helpers ----------------------------------------------------------------

function tas.shared.increment(key, delta)
  delta = delta or 1
  local value = tas.shared.get(key, 0)
  if type(value) ~= "number" then
    error("tas.shared.increment: value of '" .. tostring(key) .. "' is not a number", 2)
  end
  value = value + delta
  tas.shared.set(key, value)
  return value
end

function tas.shared.decrement(key, delta)
  return tas.shared.increment(key, -(delta or 1))
end

function tas.shared.get_or_set(key, default)
  if tas.shared.has(key) then
    return tas.shared.get(key)
  end
  tas.shared.set(key, default)
  return default
end

-- Menu navigation ------------------------------------------------------------------

local menu = tas.menu

local function normalizeScreen(name)
  name = string.lower(name)
  name = name:gsub("^menu_", "")
  name = name:gsub("^opt_", "options_")
  return name
end

-- Buttons that lead to a known screen. "back" from options/highscore depends on where
-- they were opened from, so it is only used when no forward route exists.
local routes = {
  main = { start = "start", highscore = "highscore", options = "options", credits = "credits" },
  start = { back = "main", zone = "zone" },
  zone = { back = "main" },
  options = { graphics = "options_graphics", keys = "options_keys", sound = "options_sound" },
  options_graphics = { back = "options" },
  options_keys = { back = "options" },
  options_sound = { back = "options" },
  credits = { back = "main" },
  pause = { options = "options", highscore = "highscore" },
  ["end"] = { options = "options", highscore = "highscore" },
}

local function firstStep(from, to)
  local queue, head, seen = { { from, nil } }, 1, { [from] = true }
  while queue[head] do
    local screen, first = queue[head][1], queue[head][2]
    head = head + 1
    for button, nextScreen in pairs(routes[screen] or {}) do
      if not seen[nextScreen] then
        local step = first or button
        if nextScreen == to then
          return step
        end
        seen[nextScreen] = true
        queue[#queue + 1] = { nextScreen, step }
      end
    end
  end
  return nil
end

local function waitForScreenChange(from, ticks)
  for _ = 1, ticks do
    tas.wait_ticks(1)
    -- Screens hand over through a frame or two where none is active.
    local current = menu.get_current()
    if current ~= nil and current ~= from then
      -- Menus switch their key and button handlers on a frame after they appear.
      tas.wait_ticks(2)
      return true
    end
  end
  return false
end

function menu.navigate_to(target)
  if type(target) ~= "string" or target == "" then
    error("tas.menu.navigate_to: expected a screen name", 2)
  end
  target = normalizeScreen(target)
  for _ = 1, 12 do
    local current = menu.get_current()
    if current == target then
      return true
    end
    if current == nil then
      return false, "no menu is active"
    end
    local button = firstStep(current, target) or "back"
    local ok, err = menu.click_button(button)
    if not ok then
      return false, "cannot reach '" .. target .. "' from '" .. current .. "': " .. tostring(err)
    end
    if not waitForScreenChange(current, 132) then
      return false, "pressing '" .. button .. "' on '" .. current .. "' did not change the screen"
    end
  end
  return false, "gave up navigating to '" .. target .. "'"
end

local exitLevelToMain = menu.go_to_main
function menu.go_to_main()
  if menu.is_in_game() then
    return exitLevelToMain()
  end
  return menu.navigate_to("main")
end

function menu.send_key(key, duration)
  duration = duration or 1
  if math.type(duration) ~= "integer" or duration < 1 then
    error("tas.menu.send_key: duration must be a positive integer", 2)
  end
  tas.hold(key, duration)
  -- One released frame, so the next press of the same key is a new edge.
  tas.wait_ticks(1)
  return true
end

function menu.press_enter()
  return menu.send_key("enter")
end

function menu.press_escape()
  return menu.send_key("esc")
end

-- The runtime calls this for every game event delivered to this context.
return dispatch
)LUA";

const char *const LuaApi::kEventDispatchKey = "BallanceTAS.DispatchEvent";

void LuaApi::RegisterPrelude(lua_State *state) {
    if (luaL_loadbufferx(state, kPrelude, sizeof(kPrelude) - 1, "=tas.prelude", "t") != LUA_OK ||
        lua_pcall(state, 0, 1, 0) != LUA_OK) {
        std::string message = lua_tostring(state, -1) ? lua_tostring(state, -1) : "unknown error";
        lua_pop(state, 1);
        throw std::runtime_error("LuaApi prelude failed: " + message);
    }
    lua_setfield(state, LUA_REGISTRYINDEX, kEventDispatchKey);
}
