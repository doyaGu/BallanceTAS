return function()
  local direct_payload = nil
  tas.message.subscribe("direct_smoke", function(payload)
    direct_payload = payload
  end)
  if not tas.message.send("global", "direct_smoke", { value = 11 }) then
    error("tas.message.send returned false")
  end
  tas.wait_ticks(1)
  if type(direct_payload) ~= "table" or direct_payload.value ~= 11 then
    error("tas.message.send delivery failed")
  end
  tas.message.unsubscribe("direct_smoke")

  local correlation = tas.message.request_async("global", "request_smoke", { value = "request" })
  if type(correlation) ~= "string" or correlation == "" then
    error("tas.message.request_async failed")
  end
end
