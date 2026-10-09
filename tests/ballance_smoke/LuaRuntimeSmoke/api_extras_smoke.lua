local function check(condition, message)
  if not condition then
    error(message, 2)
  end
end

local function near(a, b)
  return math.abs(a - b) < 1e-4
end

local function events()
  local calls = {}
  local id = tas.on("smoke_event", function(a, b) calls[#calls + 1] = a + b end)
  tas.once("smoke_event", function() calls[#calls + 1] = "once" end)
  check(tas.get_listener_count("smoke_event") == 2 and tas.has_listeners("smoke_event"), "listener count")

  tas.fire_event("smoke_event", 1, 2)
  tas.fire_event("smoke_event", 3, 4)
  check(#calls == 3 and calls[1] == 3 and calls[2] == "once" and calls[3] == 7, "fire_event dispatch order")
  check(tas.get_listener_count("smoke_event") == 1, "once listener should be gone")

  check(tas.off(id) and not tas.off(id), "tas.off")
  check(not tas.has_listeners("smoke_event"), "listeners after off")

  -- A failing listener is logged and does not stop the others.
  local reached = false
  tas.on("smoke_error", function() error("expected listener failure", 0) end)
  tas.on("smoke_error", function() reached = true end)
  tas.fire_event("smoke_error")
  check(reached, "listener after a failing one should still run")
  tas.clear_listeners("smoke_error")
  check(tas.get_listener_count("smoke_error") == 0, "clear_listeners(name)")

  -- Listeners start immediately and may wait.
  local phase = nil
  tas.once("smoke_wait", function(value)
    phase = "started:" .. value
    tas.wait_ticks(2)
    phase = "finished"
  end)
  tas.fire_event("smoke_wait", "x")
  check(phase == "started:x", "listener should run synchronously until its first wait")
  tas.wait_ticks(4)
  check(phase == "finished", "waiting listener should resume on later ticks")
end

local function flow()
  local start = tas.get_tick()
  local seen = 0
  tas.repeat_for(function() seen = seen + 1 end, 3)
  check(seen == 3 and tas.get_tick() - start >= 3, "repeat_for")

  seen = 0
  tas.repeat_count(function() seen = seen + 1 end, 4)
  check(seen == 4, "repeat_count")

  seen = 0
  tas.repeat_until(function() seen = seen + 1 end, function() return seen >= 2 end)
  check(seen == 2, "repeat_until")

  seen = 0
  tas.repeat_while(function() seen = seen + 1 end, function() return seen < 3 end)
  check(seen == 3, "repeat_while")

  local ok, value = tas.timeout(function() tas.wait_ticks(1); return 42 end, 10)
  check(ok and value == 42, "timeout success")
  local timedOut, reason = tas.timeout(function() tas.wait_ticks(50) end, 2)
  check(not timedOut and reason == "timeout", "timeout expiry")

  local attempts = 0
  local result = tas.retry(function()
    attempts = attempts + 1
    if attempts < 3 then error("not yet") end
    return "done"
  end, 5)
  check(result == "done" and attempts == 3, "retry")
  check(not pcall(tas.retry, function() error("always") end, 2), "retry should rethrow the last error")
end

local function logging_and_shared()
  tas.log("API Extras format %d %s", 7, "ok")
  tas.warn("API Extras warn check")
  -- Not called: the smoke runner treats any error-level line as a failure.
  check(type(tas.error) == "function", "tas.error")

  tas.shared.remove("smoke_counter")
  check(tas.shared.increment("smoke_counter") == 1, "shared.increment default")
  check(tas.shared.increment("smoke_counter", 4) == 5, "shared.increment delta")
  check(tas.shared.decrement("smoke_counter", 2) == 3, "shared.decrement")
  check(tas.shared.get_or_set("smoke_counter", 99) == 3, "shared.get_or_set existing")
  tas.shared.remove("smoke_new")
  check(tas.shared.get_or_set("smoke_new", "x") == "x" and tas.shared.get("smoke_new") == "x", "shared.get_or_set new")
  tas.shared.remove("smoke_counter")
  tas.shared.remove("smoke_new")
end

local function math_extras()
  local v = VxVector(1, 0, 0)
  local r = v:rotate(VxVector(0, 0, 1), math.pi / 2)
  check(near(r.x, 0) and near(r.y, 1) and near(r.z, 0), "VxVector:rotate")
  local m = VxVector(1, -1, 0):reflect(VxVector(0, 1, 0))
  check(near(m.x, 1) and near(m.y, 1), "VxVector:reflect")
  local i = VxVector(0, 0, 0):interpolate(VxVector(10, 20, 30), 0.5)
  check(near(i.x, 5) and near(i.y, 10) and near(i.z, 15), "VxVector:interpolate")

  local q = VxQuaternion()
  q:from_rotation(VxVector(0, 1, 0), 0.5)
  local back = VxQuaternion()
  back:from_matrix(q:to_matrix())
  check(math.abs(q:dot(back)) > 0.9999, "VxQuaternion matrix round trip")

  check(tas.result.err("bad"):unwrap_err() == "bad", "Result:unwrap_err")
  check(tas.result.err("why"):unwrap_or_else(function(e) return "got " .. e end) == "got why", "unwrap_or_else gets the error")
  local color = VxColor(0, 0, 0, 1)
  check(color:set(1, 0, 0) == color and near((2 * color).r, 2), "VxColor:set returns self; number * color")
  check(VxQuaternion(0, 0, 0, 1)[7] == nil, "VxQuaternion out-of-range index gives nil")
  check(not pcall(tas.wait, 1.5), "tas.wait with a fractional tick count should fail")
  check(not pcall(function() return tas.result.ok(1):unwrap_err() end), "unwrap_err on Ok should fail")
end

local function entity()
  local camera = tas.get_camera()
  check(camera ~= nil, "camera should exist at the menu")
  local position = camera:get_position()
  local world = camera:get_world_matrix()
  check(type(world.determinant) == "number", "get_world_matrix")
  local back = camera:inverse_transform(camera:transform(VxVector(1, 2, 3)))
  check(near(back.x, 1) and near(back.y, 2) and near(back.z, 3), "transform / inverse_transform round trip")
  local dir, up, right = camera:get_orientation()
  check(dir ~= nil and up ~= nil and right ~= nil, "get_orientation")
  check(camera:get_quaternion() ~= nil, "get_quaternion")

  camera:set_position(position + VxVector(0, 1, 0))
  check(near(camera:get_position().y, position.y + 1), "set_position")
  camera:set_position(position)
  check(near(camera:get_position().y, position.y), "set_position restore")

  check(type(camera.is_hidden_by_parent) == "boolean" and type(camera.pickable) == "boolean", "CKObject/CK3dEntity flags")
  local box = camera:get_bounding_box()
  check(box ~= nil, "get_bounding_box")
end

local function record_editing()
  local r = tas.record
  check(not r.is_playing(), "record editing smoke expects no record playback")
  local base = r.get_total_frames()
  check(r.insert_frames(base, 10), "insert_frames")
  local first = base
  check(r.set_frame_key(first + 2, "up", true), "set_frame_key")
  check(r.add_marker(first + 2, "smoke_jump", r.MarkerType.TRICK, "jump here"), "add_marker")
  check(r.get_marker("smoke_jump").type == r.MarkerType.TRICK, "get_marker")
  check(r.add_section(first, first + 4, "smoke_intro"), "add_section")
  check(r.get_section_at_frame(first + 1).name == "smoke_intro", "get_section_at_frame")
  local commentId = r.add_comment(first + 3, "note")
  check(r.get_comment(commentId).text == "note", "get_comment")
  local otherId = r.add_comment(first + 1, "other")
  check(r.remove_comment(commentId) and r.get_comment(otherId).text == "other", "comment ids survive removals")
  check(r.undo() and r.get_comment(commentId).text == "note", "undo comment removal")
  check(r.undo(), "undo second comment")
  check(#r.find_frames_with_keys("up", first) == 1, "find_frames_with_keys")
  check(#r.find_frames_with_keys("up space", first) == 0, "find_frames_with_keys requires every key")
  check(r.get_next_marker(first) ~= nil and r.get_next_marker(first, r.MarkerType.BOOKMARK) == nil, "marker type filter")
  check(type(r.get_metadata().custom) == "table", "get_metadata().custom")
  check(r.get_pressed_keys(first + 2)[1] == "up", "get_pressed_keys")

  check(r.can_undo(), "can_undo")
  check(r.undo() and r.get_comment(commentId) == nil, "undo comment")
  check(r.undo() and r.get_section("smoke_intro") == nil, "undo section")
  check(r.undo() and r.get_marker("smoke_jump") == nil, "undo marker")
  check(r.undo(), "undo set_frame_key")
  local frame = r.get_frame_input(first + 2)
  check(frame and not frame.keys.up, "undo restored the key")
  check(r.redo() and r.get_frame_input(first + 2).keys.up, "redo")

  check(r.create_branch("smoke_alt", first) and r.switch_branch("smoke_alt"), "switch to branch")
  check(r.set_frame_key(first + 5, "space", true), "edit branch")
  check(r.switch_branch("") and not r.get_frame_input(first + 5).keys.space, "main branch untouched")
  check(r.delete_branch("smoke_alt"), "delete_branch")

  check(r.delete_frames(first, 10) and r.get_total_frames() == base, "cleanup frames")
  r.clear_history()
end

return function()
  events()
  flow()
  logging_and_shared()
  math_extras()
  entity()
  record_editing()
end
