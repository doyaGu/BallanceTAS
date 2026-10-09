local util = require("smoke_util")
local check, expect_ok = util.check, util.expect_ok

-- tas.shared and tas.global are the same store in two namespaces.
local function store_roundtrip(store, label)
  store.clear()
  expect_ok(label .. ".set number", store.set("smoke_number", 42))
  expect_ok(label .. ".set table", store.set("smoke_table", { nested = { ok = true }, precise = 1.25 }))
  check(store.get("smoke_number") == 42, label .. " number roundtrip")
  local value = store.get("smoke_table")
  check(type(value) == "table" and value.nested.ok == true and value.precise == 1.25, label .. " table roundtrip")
  check(store.get("missing", "fallback") == "fallback", label .. ".get default")
  check(store.has("smoke_table") == true and store.has("missing") == false, label .. ".has")
  check(#store.keys() == 2, label .. ".keys")

  expect_ok(label .. ".remove", store.remove("smoke_number"))
  check(store.has("smoke_number") == false, label .. ".remove took effect")
  local ok, reason = store.remove("smoke_number")
  check(ok == false and type(reason) == "string", label .. ".remove of a missing key reports why")

  local stored = pcall(store.set, "fn", function() end)
  check(not stored, label .. ".set should refuse functions")
  check(not pcall(store.get, ""), label .. ".get should refuse an empty key")
  store.clear()
  check(#store.keys() == 0, label .. ".clear")
end

return function()
  store_roundtrip(tas.global, "tas.global")
  store_roundtrip(tas.shared, "tas.shared")

  -- The namespaces are separate.
  tas.shared.set("both", "shared")
  tas.global.set("both", "global")
  check(tas.shared.get("both") == "shared" and tas.global.get("both") == "global", "namespaces are separate")
  tas.shared.clear()
  tas.global.clear()

  local watched = false
  tas.shared.watch("watch_key", function(new_value, old_value, key)
    watched = new_value == "new" and old_value == nil and key == "watch_key"
  end)
  tas.shared.set("watch_key", "new")
  tas.wait_ticks(1)
  check(watched, "tas.shared.watch")
  tas.shared.unwatch("watch_key")
  tas.shared.clear()
end
