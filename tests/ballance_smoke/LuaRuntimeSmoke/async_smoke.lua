return function()
  local asyncTask = tas.async(function()
    return 123
  end)
  if asyncTask:is_done() then
    error("async task completed before await")
  end
  if asyncTask:await() ~= 123 or not asyncTask:is_completed() then
    error("AsyncTask.await failed")
  end

  local asyncSpawn = tas.async.spawn(function()
    return "spawned"
  end)
  if tas.await(asyncSpawn) ~= "spawned" then
    error("tas.async.spawn/tas.await failed")
  end

  local asyncAll = tas.async.all({
    tas.async.create(function() return 1 end),
    tas.async.create(function() return 2 end),
  })
  if asyncAll[1] ~= 1 or asyncAll[2] ~= 2 then
    error("tas.async.all order/result failed")
  end

  local asyncStartTick = tas.get_tick()
  local delayedTask = tas.async.spawn(function()
    tas.wait_ticks(2)
    return tas.get_tick() - asyncStartTick
  end)
  local delayedTicks = tas.await(delayedTask)
  if type(delayedTicks) ~= "number" or delayedTicks < 2 then
    error("async scheduler delay failed: " .. tostring(delayedTicks))
  end

  local asyncGate = false
  local waitUntilTask = tas.async.spawn(function()
    tas.async.wait_until(function() return asyncGate end)
    return "wait_until_ok"
  end)
  tas.wait_ticks(1)
  if waitUntilTask:is_done() then
    error("tas.async.wait_until completed too early")
  end
  asyncGate = true
  if tas.await(waitUntilTask) ~= "wait_until_ok" then
    error("tas.async.wait_until failed")
  end

  local raceLoserRan = false
  local asyncRace = tas.async.race({
    tas.async.spawn(function() tas.wait_ticks(3); raceLoserRan = true; return "slow" end),
    tas.async.spawn(function() tas.wait_ticks(1); return "fast" end),
  })
  if type(asyncRace) ~= "table" or asyncRace.index ~= 2 or asyncRace.value ~= "fast" then
    error("tas.async.race failed")
  end
  if asyncRace.status ~= "completed" then
    error("tas.async.race missing completed status")
  end
  tas.wait_ticks(4)
  if raceLoserRan then
    error("tas.async.race did not cancel losing task")
  end

  local anyLoserRan = false
  local asyncAny = tas.async.any({
    tas.async.spawn(function() tas.wait_ticks(3); anyLoserRan = true; return "late" end),
    tas.async.spawn(function() tas.wait_ticks(1); return "any_ok" end),
  })
  if type(asyncAny) ~= "table" or asyncAny.index ~= 2 or asyncAny.value ~= "any_ok" then
    error("tas.async.any failed")
  end
  if asyncAny.status ~= "completed" then
    error("tas.async.any missing completed status")
  end
  tas.wait_ticks(4)
  if anyLoserRan then
    error("tas.async.any did not cancel losing task")
  end

  local ok, awaitErr = pcall(function()
    return tas.await(tas.async.spawn(function()
      error("typed await failure")
    end))
  end)
  if ok or type(awaitErr) ~= "table" or awaitErr.kind ~= "failed" or not tostring(awaitErr.message):find("typed await failure", 1, true) then
    error("tas.await failed task did not raise typed error: ok="
      .. tostring(ok)
      .. " err_type=" .. type(awaitErr)
      .. " kind=" .. tostring(type(awaitErr) == "table" and awaitErr.kind or nil)
      .. " message=" .. tostring(type(awaitErr) == "table" and awaitErr.message or awaitErr)
      .. " traceback=" .. tostring(type(awaitErr) == "table" and awaitErr.traceback or nil))
  end

  local allOk, allErr = pcall(function()
    return tas.async.all({
      tas.async.create(function() return "unused" end),
      tas.async.create(function() error("typed all failure") end),
    })
  end)
  if allOk or type(allErr) ~= "table" or allErr.kind ~= "failed" or allErr.index ~= 2 then
    error("tas.async.all failed task did not raise indexed typed error: ok="
      .. tostring(allOk)
      .. " err_type=" .. type(allErr)
      .. " kind=" .. tostring(type(allErr) == "table" and allErr.kind or nil)
      .. " index=" .. tostring(type(allErr) == "table" and allErr.index or nil)
      .. " message=" .. tostring(type(allErr) == "table" and allErr.message or allErr))
  end

  local raceFailureLoserRan = false
  local raceOk, raceErr = pcall(function()
    return tas.async.race({
      tas.async.create(function() error("typed race failure") end),
      tas.async.create(function() tas.wait_ticks(5); raceFailureLoserRan = true; return "too late" end),
    })
  end)
  if raceOk or type(raceErr) ~= "table" or raceErr.kind ~= "failed" or raceErr.index ~= 1 then
    error("tas.async.race failed winner did not raise indexed typed error: ok="
      .. tostring(raceOk)
      .. " err_type=" .. type(raceErr)
      .. " kind=" .. tostring(type(raceErr) == "table" and raceErr.kind or nil)
      .. " index=" .. tostring(type(raceErr) == "table" and raceErr.index or nil)
      .. " status=" .. tostring(type(raceErr) == "table" and raceErr.status or nil))
  end
  tas.wait_ticks(4)
  if raceFailureLoserRan then
    error("tas.async.race did not cancel loser after failed winner")
  end

  local anySkipFailure = tas.async.any({
    tas.async.spawn(function() tas.wait_ticks(1); error("typed any ignored failure") end),
    tas.async.spawn(function() tas.wait_ticks(2); return "any_survived" end),
  })
  if anySkipFailure.index ~= 2 or anySkipFailure.value ~= "any_survived" then
    error("tas.async.any did not skip failed task")
  end

  local anyOk, anyErr = pcall(function()
    return tas.async.any({
      tas.async.create(function() error("typed any first failure") end),
      tas.async.create(function() error("typed any second failure") end),
    })
  end)
  if anyOk or type(anyErr) ~= "table" or anyErr.kind ~= "all_failed" or type(anyErr.errors) ~= "table" or #anyErr.errors ~= 2 then
    error("tas.async.any all-failed did not raise aggregate typed error")
  end
end
