local function read_reference_path()
  local candidates = {
    "validation_reference.txt",
    "ModLoader/TAS/LuaRuntimeSmoke/validation_reference.txt",
    "../ModLoader/TAS/LuaRuntimeSmoke/validation_reference.txt",
  }

  for _, path in ipairs(candidates) do
    local file = io.open(path, "r")
    if file then
      local value = file:read("*a") or ""
      file:close()
      value = value:match("^%s*(.-)%s*$")
      if value ~= "" then
        return value
      end
    end
  end
  return nil
end

local function start_record_driver()
  for _, project in ipairs(tas.project.list()) do
    if project.type == "record" then
      if not tas.project.load(project.name) then
        error("failed to load validation record driver: " .. project.name)
      end
      if not tas.record.is_playing() then
        error("record driver did not enter playing state: " .. project.name)
      end
      return project.name
    end
  end
  error("validation smoke requires at least one record project")
end

return function()
  if type(tas.validation) ~= "table" then
    error("tas.validation table missing")
  end
  if tas.validation.is_active() then
    error("validation unexpectedly active before smoke")
  end


  local driver_name = start_record_driver()
  tas.log("Validation Smoke DRIVER=" .. driver_name)

  local reference_path = read_reference_path()
  local start_error = tas.validation.start(reference_path)
  if start_error ~= nil then
    error("tas.validation.start failed: " .. tostring(start_error))
  end
  if not tas.validation.is_active() then
    error("validation did not become active")
  end

  tas.wait_ticks(4)

  local report = tas.validation.stop()
  tas.record.stop()
  if type(report) ~= "table" then
    error("tas.validation.stop did not return a report")
  end
  if tas.validation.is_active() then
    error("validation remained active after stop")
  end
  if type(report.dump_path) ~= "string" or report.dump_path == "" then
    error("validation report missing dump_path")
  end
  if type(report.live_frames) ~= "number" or report.live_frames < 1 then
    error("validation captured no live frames")
  end

  tas.log("Validation Smoke DUMP=" .. report.dump_path)

  if reference_path then
    if report.has_reference ~= true then
      error("validation did not load the supplied reference")
    end
    if report.identical ~= true then
      error("validation round-trip diverged at frame " ..
        tostring(report.first_divergence_frame))
    end
    if type(report.report_path) ~= "string" or report.report_path == "" then
      error("validation comparison report missing report_path")
    end
    tas.log("Validation Smoke MATCH")
  else
    if report.has_reference ~= false or report.report_path ~= nil then
      error("reference-free validation report fields are inconsistent")
    end
    tas.log("Validation Smoke REFERENCE CREATED")
  end
end
