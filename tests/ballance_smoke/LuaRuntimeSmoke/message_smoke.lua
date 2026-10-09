return function()
  local direct_payload = nil
  local direct_message = nil
  tas.message.subscribe("direct_smoke", function(payload, message)
    direct_payload = payload
    direct_message = message
  end)
  if not tas.message.send("global", "direct_smoke", { value = 11 }) then
    error("tas.message.send returned false")
  end
  tas.wait_ticks(1)
  if type(direct_payload) ~= "table" or direct_payload.value ~= 11 then
    error("tas.message.send delivery failed")
  end
  if type(direct_message) ~= "table" or direct_message.sender ~= "global" or direct_message.type ~= "direct_smoke"
      or direct_message.data.value ~= 11 or direct_message.is_request ~= false then
    error("message handlers should receive the message table as their second argument")
  end
  tas.message.unsubscribe("direct_smoke")

  -- A request carries a correlation id the handler can answer with.
  local request_message = nil
  tas.message.subscribe("request_smoke", function(_, message)
    request_message = message
  end)

  local correlation = tas.message.request_async("global", "request_smoke", { value = "request" })
  if type(correlation) ~= "string" or correlation == "" then
    error("tas.message.request_async failed")
  end
  tas.wait_ticks(1)
  if type(request_message) ~= "table" or request_message.correlation_id ~= correlation or request_message.is_request ~= true then
    error("requests should reach the handler with their correlation id")
  end
  if not tas.message.respond(request_message.sender, request_message.correlation_id, { ok = true }) then
    error("tas.message.respond failed")
  end
  tas.message.unsubscribe("request_smoke")
end
