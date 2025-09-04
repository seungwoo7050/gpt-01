-- [SEQUENCE: MVP8-16] A simple Lua script to test the C++/Lua binding.

print("Hello from Lua!")
native_print("This message is printed by a C++ function called from Lua.")

-- In a real scenario, you might do something like:
-- local player = GetPlayer("Gemini")
-- if player then
--   player:move_to(100, 200)
--   native_print("Player " .. player:get_name() .. " moved.")
-- end
