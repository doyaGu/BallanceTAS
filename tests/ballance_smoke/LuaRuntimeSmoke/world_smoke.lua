return function()
  local object = nil
  local camera = nil
  for _ = 1, 60 do
    camera = tas.get_camera()
    if camera ~= nil then
      object = camera
      break
    end
    tas.wait_ticks(1)
  end
  if object == nil then
    for id = 1, 5000 do
      object = tas.get_object_by_id(id)
      if object ~= nil then
        break
      end
    end
  end
  if object == nil then
    error("world object query returned nil")
  end
  if type(object.id) ~= "number" or object.id <= 0 then
    error("CKObject id failed")
  end
  if type(object.name) ~= "string" then
    error("CKObject name failed")
  end
  local sameObject = tas.get_object_by_id(object.id)
  if sameObject == nil or sameObject.id ~= object.id then
    error("tas.get_object_by_id failed")
  end
  local objectPosition = object:get_position()
  if objectPosition == nil or type(objectPosition.x) ~= "number" then
    error("CK3dEntity get_position failed")
  end
  if camera ~= nil then
    if type(camera.front_plane) ~= "number" or type(camera.fov) ~= "number" then
      error("CKCamera properties failed")
    end
    local projection = camera:compute_projection_matrix()
    if type(projection) ~= "userdata" or type(projection.determinant) ~= "number" then
      error("CKCamera projection matrix failed")
    end
  end
end
