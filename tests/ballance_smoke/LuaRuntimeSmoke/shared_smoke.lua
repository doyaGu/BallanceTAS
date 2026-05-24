return function()
  tas.global.clear()
  tas.shared.clear()

  tas.global.set("smoke_number", 42)
  tas.global.set("smoke_table", { nested = { ok = true }, name = "global" })
  if tas.global.get("smoke_number") ~= 42 then
    error("tas.global number roundtrip failed")
  end
  local global_table = tas.global.get("smoke_table")
  if type(global_table) ~= "table" or global_table.nested.ok ~= true then
    error("tas.global table roundtrip failed")
  end
  if tas.global.has("smoke_table") ~= true then
    error("tas.global.has failed")
  end
  if #tas.global.keys() < 2 then
    error("tas.global.keys failed")
  end

  tas.shared.set("shared_number", 7)
  tas.shared.set("shared_table", { nested = { ok = true }, precise = 1.25 })
  if tas.shared.get("shared_number") ~= 7 then
    error("tas.shared number roundtrip failed")
  end
  local shared_table = tas.shared.get("shared_table")
  if type(shared_table) ~= "table" or shared_table.nested.ok ~= true or shared_table.precise ~= 1.25 then
    error("tas.shared table roundtrip failed")
  end
  if tas.shared.has("shared_table") ~= true then
    error("tas.shared.has failed")
  end
  if #tas.shared.keys() < 2 then
    error("tas.shared.keys failed")
  end

  local watched = false
  tas.shared.watch("watch_key", function(new_value, old_value, key)
    watched = new_value == "new" and old_value == nil and key == "watch_key"
  end)
  tas.shared.set("watch_key", "new")
  tas.wait_ticks(1)
  if not watched then
    error("tas.shared.watch failed")
  end
  tas.shared.unwatch("watch_key")

  tas.global.remove("smoke_number")
  if tas.global.has("smoke_number") ~= false then
    error("tas.global.remove failed")
  end
  tas.shared.remove("shared_number")
  if tas.shared.has("shared_number") ~= false then
    error("tas.shared.remove failed")
  end
end
