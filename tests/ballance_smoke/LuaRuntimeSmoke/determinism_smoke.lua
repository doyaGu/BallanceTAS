return function()
  local detErr = tas.determinism.start_recording()
  if detErr ~= nil then
    error("tas.determinism.start_recording failed: " .. tostring(detErr))
  end
  tas.wait_ticks(2)
  local detStatus = tas.determinism.get_status()
  if type(detStatus) ~= "table" or detStatus.mode ~= "recording" or detStatus.ticks_processed < 1 then
    error("tas.determinism.get_status failed")
  end
  if type(detStatus.current_path) ~= "string" or detStatus.current_path == "" then
    error("tas.determinism.get_status missing current_path")
  end
  if detStatus.diverged ~= false or detStatus.divergence_tick ~= nil then
    error("tas.determinism.get_status divergence fields invalid")
  end
  if type(tas.determinism.get_current_hash()) ~= "string" then
    error("tas.determinism.get_current_hash failed")
  end
  tas.determinism.stop()
  if tas.determinism.get_status().mode ~= "idle" then
    error("tas.determinism.stop failed")
  end
end
