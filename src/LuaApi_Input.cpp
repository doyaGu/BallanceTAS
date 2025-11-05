#include "LuaApi.h"

#include "Logger.h"
#include <stdexcept>

#include "TASEngine.h"
#include "InputSystem.h"
#include "LuaScheduler.h"
#include "ScriptContext.h"

// ===================================================================
//  Input API Registration
// ===================================================================

void LuaApi::RegisterInputApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterInputApi requires a valid ScriptContext");
    }

    auto *inputSystem = context->GetInputSystem();
    if (!inputSystem) {
        throw std::runtime_error("LuaApi::RegisterInputApi: Input system not available for this context");
    }

    auto *scheduler = context->GetScheduler();
    if (!scheduler) {
        throw std::runtime_error("LuaApi::RegisterInputApi: Scheduler not available for this context");
    }

    // ===================================================================
    // Keyboard Input API (tas.keyboard.*)
    // ===================================================================

    sol::table keyboard = tas["keyboard"] = tas.create();

    // tas.keyboard.press(key_string)
    keyboard["press"] = [inputSystem](const std::string &keyString) {
        if (keyString.empty()) {
            throw sol::error("keyboard.press: key string cannot be empty");
        }
        // Press keys for exactly one frame
        inputSystem->PressKeysOneFrame(keyString);
    };

    // tas.keyboard.hold(key_string, duration_ticks)
    keyboard["hold"] = sol::yielding([inputSystem, scheduler](const std::string &keyString, int duration) {
        if (keyString.empty()) {
            throw sol::error("keyboard.hold: key string cannot be empty");
        }
        if (duration <= 0) {
            throw sol::error("keyboard.hold: duration must be positive");
        }
        // InputSystem handles the timing internally
        inputSystem->HoldKeys(keyString, duration);
        // Yield for the specified duration
        scheduler->YieldTicks(duration);
    });

    // tas.keyboard.key_down(key_string)
    keyboard["key_down"] = [inputSystem](const std::string &keyString) {
        if (keyString.empty()) {
            throw sol::error("keyboard.key_down: key string cannot be empty");
        }
        inputSystem->PressKeys(keyString);
    };

    // tas.keyboard.key_up(key_string)
    keyboard["key_up"] = [inputSystem](const std::string &keyString) {
        if (keyString.empty()) {
            throw sol::error("keyboard.key_up: key string cannot be empty");
        }
        inputSystem->ReleaseKeys(keyString);
    };

    // tas.keyboard.release_all()
    keyboard["release_all"] = [inputSystem]() {
        inputSystem->ReleaseAllKeys();
    };

    // tas.keyboard.are_keys_down(key_string)
    keyboard["are_keys_down"] = [inputSystem](const std::string &keyString) {
        if (keyString.empty()) {
            return false;
        }
        return inputSystem->AreKeysDown(keyString);
    };

    // tas.keyboard.are_keys_up(key_string)
    keyboard["are_keys_up"] = [inputSystem](const std::string &keyString) {
        if (keyString.empty()) {
            return false;
        }
        return inputSystem->AreKeysUp(keyString);
    };

    // tas.keyboard.are_keys_toggled(key_string)
    keyboard["are_keys_toggled"] = [inputSystem](const std::string &keyString) -> bool {
        if (keyString.empty()) {
            return false;
        }
        return inputSystem->AreKeysToggled(keyString);
    };

    // tas.keyboard.get_available_keys()
    keyboard["get_available_keys"] = [inputSystem]() -> std::vector<std::string> {
        return inputSystem->GetAvailableKeys();
    };

    // ===================================================================
    // Keyboard API Aliases (Backward Compatibility)
    // ===================================================================

    // tas.press(key_string) - Alias for tas.keyboard.press
    tas["press"] = keyboard["press"];

    // tas.hold(key_string, duration_ticks) - Alias for tas.keyboard.hold
    tas["hold"] = keyboard["hold"];

    // tas.key_down(key_string) - Alias for tas.keyboard.key_down
    tas["key_down"] = keyboard["key_down"];

    // tas.key_up(key_string) - Alias for tas.keyboard.key_up
    tas["key_up"] = keyboard["key_up"];

    // tas.release_all_keys() - Alias for tas.keyboard.release_all
    tas["release_all_keys"] = keyboard["release_all"];

    // tas.are_keys_down(key_string) - Alias for tas.keyboard.are_keys_down
    tas["are_keys_down"] = keyboard["are_keys_down"];

    // tas.are_keys_up(key_string) - Alias for tas.keyboard.are_keys_up
    tas["are_keys_up"] = keyboard["are_keys_up"];

    // tas.are_keys_toggled(key_string) - Alias for tas.keyboard.are_keys_toggled
    tas["are_keys_toggled"] = keyboard["are_keys_toggled"];

    // ===================================================================
    // Mouse Input API (tas.mouse.*)
    // ===================================================================

    sol::table mouse = tas["mouse"] = tas.create();

    // Convert button name to index
    auto getMouseButtonIndex = [](const std::string &button) -> int {
        if (button == "left") return 0;
        if (button == "right") return 1;
        if (button == "middle") return 2;
        if (button == "x1") return 3;
        throw sol::error("Invalid mouse button: " + button);
    };

    // tas.mouse.press(button)
    mouse["press"] = [inputSystem, getMouseButtonIndex](const std::string &button) {
        if (button.empty()) {
            throw sol::error("mouse.press: button cannot be empty");
        }
        int buttonIndex = getMouseButtonIndex(button);
        inputSystem->PressMouseButtonOneFrame(buttonIndex);
    };

    // tas.mouse.hold(button, duration_ticks)
    mouse["hold"] = sol::yielding(
        [inputSystem, scheduler, getMouseButtonIndex](const std::string &button, int duration) {
            if (button.empty()) {
                throw sol::error("mouse.hold: button cannot be empty");
            }
            if (duration <= 0) {
                throw sol::error("mouse.hold: duration must be positive");
            }
            int buttonIndex = getMouseButtonIndex(button);
            inputSystem->HoldMouseButton(buttonIndex, duration);
            scheduler->YieldTicks(duration);
        });

    // tas.mouse.button_down(button)
    mouse["button_down"] = [inputSystem, getMouseButtonIndex](const std::string &button) {
        if (button.empty()) {
            throw sol::error("mouse.button_down: button cannot be empty");
        }
        int buttonIndex = getMouseButtonIndex(button);
        inputSystem->PressMouseButton(buttonIndex);
    };

    // tas.mouse.button_up(button)
    mouse["button_up"] = [inputSystem, getMouseButtonIndex](const std::string &button) {
        if (button.empty()) {
            throw sol::error("mouse.button_up: button cannot be empty");
        }
        int buttonIndex = getMouseButtonIndex(button);
        inputSystem->ReleaseMouseButton(buttonIndex);
    };

    // tas.mouse.set_position(x, y)
    mouse["set_position"] = [inputSystem](float x, float y) {
        inputSystem->SetMousePosition(x, y);
    };

    // tas.mouse.move(dx, dy)
    mouse["move"] = [inputSystem](float dx, float dy) {
        inputSystem->MoveMouseRelative(dx, dy);
    };

    // tas.mouse.set_wheel(delta)
    mouse["set_wheel"] = [inputSystem](int delta) {
        inputSystem->SetMouseWheel(delta);
    };

    // tas.mouse.is_button_down(button)
    mouse["is_button_down"] = [inputSystem, getMouseButtonIndex](const std::string &button) -> bool {
        if (button.empty()) {
            return false;
        }
        int buttonIndex = getMouseButtonIndex(button);
        return inputSystem->IsMouseButtonDown(buttonIndex);
    };

    // tas.mouse.is_button_up(button)
    mouse["is_button_up"] = [inputSystem, getMouseButtonIndex](const std::string &button) -> bool {
        if (button.empty()) {
            return false;
        }
        int buttonIndex = getMouseButtonIndex(button);
        return inputSystem->IsMouseButtonUp(buttonIndex);
    };

    // tas.mouse.get_position()
    mouse["get_position"] = [inputSystem, context]() -> sol::object {
        Vx2DVector pos = inputSystem->GetMousePosition();
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();
        result["x"] = pos.x;
        result["y"] = pos.y;
        return result;
    };

    // tas.mouse.get_wheel_delta()
    mouse["get_wheel_delta"] = [inputSystem]() -> int {
        return inputSystem->GetMouseWheelDelta();
    };

    // ===================================================================
    // Joystick Input API (tas.joystick.*)
    // ===================================================================

    sol::table joystick = tas["joystick"] = tas.create();

    // tas.joystick.press(joystick_id, button)
    joystick["press"] = [inputSystem](int joystickId, int button) {
        if (joystickId < 0) {
            throw sol::error("joystick.press: joystick_id must be non-negative");
        }
        if (button < 0) {
            throw sol::error("joystick.press: button must be non-negative");
        }
        // Add upper bound check (DirectInput supports up to 128 buttons, but typical is 32)
        if (button >= 128) {
            throw sol::error("joystick.press: button index out of range (max 127)");
        }
        inputSystem->PressJoystickButtonOneFrame(joystickId, button);
    };

    // tas.joystick.hold(joystick_id, button, duration_ticks)
    joystick["hold"] = sol::yielding([inputSystem, scheduler](int joystickId, int button, int duration) {
        if (joystickId < 0) {
            throw sol::error("joystick.hold: joystick_id must be non-negative");
        }
        if (button < 0) {
            throw sol::error("joystick.hold: button must be non-negative");
        }
        if (button >= 128) {
            throw sol::error("joystick.hold: button index out of range (max 127)");
        }
        if (duration <= 0) {
            throw sol::error("joystick.hold: duration must be positive");
        }
        inputSystem->HoldJoystickButton(joystickId, button, duration);
        scheduler->YieldTicks(duration);
    });

    // tas.joystick.button_down(joystick_id, button)
    joystick["button_down"] = [inputSystem](int joystickId, int button) {
        if (joystickId < 0) {
            throw sol::error("joystick.button_down: joystick_id must be non-negative");
        }
        if (button < 0) {
            throw sol::error("joystick.button_down: button must be non-negative");
        }
        if (button >= 128) {
            throw sol::error("joystick.button_down: button index out of range (max 127)");
        }
        inputSystem->PressJoystickButton(joystickId, button);
    };

    // tas.joystick.button_up(joystick_id, button)
    joystick["button_up"] = [inputSystem](int joystickId, int button) {
        if (joystickId < 0) {
            throw sol::error("joystick.button_up: joystick_id must be non-negative");
        }
        if (button < 0) {
            throw sol::error("joystick.button_up: button must be non-negative");
        }
        if (button >= 128) {
            throw sol::error("joystick.button_up: button index out of range (max 127)");
        }
        inputSystem->ReleaseJoystickButton(joystickId, button);
    };

    // tas.joystick.set_position(joystick_id, x, y, z)
    joystick["set_position"] = sol::overload(
        [inputSystem](int joystickId, float x, float y, float z) {
            if (joystickId < 0) {
                throw sol::error("joystick.set_position: joystick_id must be non-negative");
            }
            inputSystem->SetJoystickPosition(joystickId, x, y, z);
        },
        [inputSystem](int joystickId, float x, float y) {
            if (joystickId < 0) {
                throw sol::error("joystick.set_position: joystick_id must be non-negative");
            }
            inputSystem->SetJoystickPosition(joystickId, x, y, 0.0f);
        }
    );

    // tas.joystick.set_rotation(joystick_id, rx, ry, rz)
    joystick["set_rotation"] = sol::overload(
        [inputSystem](int joystickId, float rx, float ry, float rz) {
            if (joystickId < 0) {
                throw sol::error("joystick.set_rotation: joystick_id must be non-negative");
            }
            inputSystem->SetJoystickRotation(joystickId, rx, ry, rz);
        },
        [inputSystem](int joystickId, float rx, float ry) {
            if (joystickId < 0) {
                throw sol::error("joystick.set_rotation: joystick_id must be non-negative");
            }
            inputSystem->SetJoystickRotation(joystickId, rx, ry, 0.0f);
        }
    );

    // tas.joystick.set_sliders(joystick_id, slider0, slider1)
    joystick["set_sliders"] = [inputSystem](int joystickId, float slider0, float slider1) {
        if (joystickId < 0) {
            throw sol::error("joystick.set_sliders: joystick_id must be non-negative");
        }
        inputSystem->SetJoystickSliders(joystickId, slider0, slider1);
    };

    // tas.joystick.set_pov(joystick_id, angle)
    joystick["set_pov"] = [inputSystem](int joystickId, float angle) {
        if (joystickId < 0) {
            throw sol::error("joystick.set_pov: joystick_id must be non-negative");
        }
        inputSystem->SetJoystickPOV(joystickId, angle);
    };

    // tas.joystick.is_button_down(joystick_id, button)
    joystick["is_button_down"] = [inputSystem](int joystickId, int button) -> bool {
        if (joystickId < 0 || button < 0 || button >= 128) {
            return false;
        }
        return inputSystem->IsJoystickButtonDown(joystickId, button);
    };

    // tas.joystick.is_button_up(joystick_id, button)
    joystick["is_button_up"] = [inputSystem](int joystickId, int button) -> bool {
        if (joystickId < 0 || button < 0 || button >= 128) {
            return false;
        }
        return inputSystem->IsJoystickButtonUp(joystickId, button);
    };

    // tas.joystick.get_position(joystick_id)
    joystick["get_position"] = [inputSystem, context](int joystickId) -> sol::object {
        if (joystickId < 0) {
            return sol::nil;
        }
        VxVector pos = inputSystem->GetJoystickPosition(joystickId);
        return sol::make_object(context->GetLuaState(), pos);
    };

    // tas.joystick.get_rotation(joystick_id)
    joystick["get_rotation"] = [inputSystem, context](int joystickId) -> sol::object {
        if (joystickId < 0) {
            return sol::nil;
        }
        VxVector rot = inputSystem->GetJoystickRotation(joystickId);
        return sol::make_object(context->GetLuaState(), rot);
    };

    // tas.joystick.get_sliders(joystick_id)
    joystick["get_sliders"] = [inputSystem, context](int joystickId) -> sol::object {
        if (joystickId < 0) {
            return sol::nil;
        }
        Vx2DVector sliders = inputSystem->GetJoystickSliders(joystickId);
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();
        result["slider0"] = sliders.x;
        result["slider1"] = sliders.y;
        return result;
    };

    // tas.joystick.get_pov(joystick_id)
    joystick["get_pov"] = [inputSystem](int joystickId) -> float {
        if (joystickId < 0) {
            return -1.0f;
        }
        return inputSystem->GetJoystickPOV(joystickId);
    };
}
