return function()
  local v = VxVector(1, 2, 3)
  if v.x ~= 1 or v.y ~= 2 or v.z ~= 3 then
    error("VxVector fields failed")
  end
  v[0], v[1], v[2] = 4, 5, 6
  local doubled = v + v
  if doubled.x ~= 8 or doubled.y ~= 10 or doubled.z ~= 12 then
    error("VxVector operator failed")
  end
  if v:dot(VxVector.axis_x()) ~= 4 then
    error("VxVector method/static failed")
  end

  local v2 = Vx2DVector(3, 4)
  if v2.magnitude ~= 5 then
    error("Vx2DVector property failed")
  end
  v2[0], v2[1] = 6, 8
  if v2:dot(Vx2DVector(1, 0)) ~= 6 then
    error("Vx2DVector method/index failed")
  end

  local v4 = VxVector4(1, 2, 3, 4)
  if v4.w ~= 4 or v4[3] ~= 4 or (v4 + VxVector4(1, 1, 1, 1)).w ~= 5 then
    error("VxVector4 failed")
  end

  if type(CK_OBJECT_FLAGS) ~= "table" or type(CK_OBJECT_FLAGS.VISIBLE) ~= "number" then
    error("CK_OBJECT_FLAGS failed")
  end

  local color = VxColor(0.25, 0.5, 0.75, 1.0)
  color.red = 128
  if type(color.rgba) ~= "number" or color.red ~= 128 then
    error("VxColor packed property failed")
  end
  color:set(0.1, 0.2, 0.3, 0.4)
  color:check()
  if math.abs(color.a - 0.4) > 0.0001 or type(VxColor.convert(1, 1, 1, 1)) ~= "number" then
    error("VxColor method/static failed")
  end

  local rect = VxRect(0, 0, 10, 20)
  rect.center = Vx2DVector(10, 10)
  rect:set_dimension(1, 2, 3, 4)
  if rect.width ~= 3 or rect.height ~= 4 or rect:is_inside(Vx2DVector(2, 3)) ~= true then
    error("VxRect failed")
  end

  local q = VxQuaternion(0, 0, 0, 1)
  q[0] = 0.25
  q:normalize()
  if type(q.magnitude) ~= "number" or type(q.conjugate) ~= "userdata" then
    error("VxQuaternion properties failed")
  end
  local q2 = q * 2
  if type(q:dot(q2)) ~= "number" or type(q2:ln()) ~= "userdata" then
    error("VxQuaternion operator/method failed")
  end

  local matrix = VxMatrix.identity()
  if tostring(matrix):find("VxMatrix", 1, true) == nil or math.abs(matrix.determinant - 1.0) > 0.001 then
    error("VxMatrix property failed")
  end
  local matrixVector = matrix:multiply_vector(VxVector(1, 2, 3))
  if matrixVector.x ~= 1 or matrixVector.y ~= 2 or matrixVector.z ~= 3 then
    error("VxMatrix vector multiplication failed")
  end

  local box = VxBbox(VxVector(0, 0, 0), VxVector(2, 4, 6))
  if box.size.x ~= 2 then
    error("VxBbox size failed")
  end
  box.center = VxVector(10, 10, 10)
  if box.center.x ~= 10 then
    error("VxBbox center failed")
  end

  local compressed = VxCompressedVector(1, 0, 0)
  compressed:set(0, 1, 0)
  if type(tostring(compressed)) ~= "string" then
    error("VxCompressedVector failed")
  end

  local compressedOld = VxCompressedVectorOld(0, 0, 1)
  compressedOld:set(1, 0, 0)
  if type(compressedOld.xa) ~= "number" then
    error("VxCompressedVectorOld failed")
  end
end
