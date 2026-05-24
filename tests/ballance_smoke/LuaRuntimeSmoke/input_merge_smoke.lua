return function()
  tas.keyboard.press("space")
  tas.wait_ticks(1)
  tas.keyboard.key_down("up")
  tas.wait(1)
  tas.keyboard.key_up("up")
end
