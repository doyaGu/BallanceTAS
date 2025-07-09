#include "LuaApi.h"

#include <sstream>
#include <iomanip>

#include "BallanceTAS.h"
#include "TASEngine.h"
#include "LuaScheduler.h"
#include "InputSystem.h"
#include "EventManager.h"
#include "GameInterface.h"
#include "ProjectManager.h"
#include "TASProject.h"
#include "DevTools.h"

// Helper function to format strings with variadic arguments
std::string FormatString(const std::string &fmt, sol::variadic_args va) {
    std::ostringstream oss;
    oss << fmt;

    // Simple implementation - just concatenate args
    // In a real implementation, you'd want proper printf-style formatting
    for (const auto &arg : va) {
        try {
            oss << " " << arg.as<std::string>();
        } catch (const std::exception &) {
            oss << " [invalid argument]";
        }
    }

    return oss.str();
}

// ===================================================================
//  Section 2: Core & Flow Control API Registration
// ===================================================================

void LuaApi::RegisterCoreApi(sol::table &tas, TASEngine *engine) {
    auto *scheduler = engine->GetScheduler();
    auto *mod = engine->GetMod();

    // tas.log(format_string, ...)
    tas["log"] = [mod](const std::string &fmt, const sol::variadic_args &va) {
        try {
            std::string text = FormatString(fmt, va);
            mod->GetLogger()->Info("[TAS] %s", text.c_str());
        } catch (const std::exception &e) {
            mod->GetLogger()->Error("[TAS] Error in log: %s", e.what());
        }
    };

    // tas.warn(format_string, ...)
    tas["warn"] = [mod](const std::string &fmt, const sol::variadic_args &va) {
        try {
            std::string text = FormatString(fmt, va);
            mod->GetLogger()->Warn("[TAS] %s", text.c_str());
        } catch (const std::exception &e) {
            mod->GetLogger()->Error("[TAS] Error in warn: %s", e.what());
        }
    };

    // tas.error(format_string, ...)
    tas["error"] = [mod](const std::string &fmt, const sol::variadic_args &va) {
        try {
            std::string text = FormatString(fmt, va);
            mod->GetLogger()->Error("[TAS] %s", text.c_str());
        } catch (const std::exception &e) {
            mod->GetLogger()->Error("[TAS] Error in error: %s", e.what());
        }
    };

    // tas.print(format_string, ...)
    tas["print"] = [engine, mod](const std::string &fmt, const sol::variadic_args &va) {
        try {
            std::string text = FormatString(fmt, va);
            engine->GetGameInterface()->PrintMessage(text.c_str());
        } catch (const std::exception &e) {
            mod->GetLogger()->Error("[TAS] Error in print: %s", e.what());
        }
    };

    // tas.wait_ticks(num_ticks)
    tas["wait_ticks"] = sol::yielding([scheduler](int ticks) {
        if (ticks <= 0) {
            throw sol::error("wait_ticks: tick count must be positive");
        }
        scheduler->YieldTicks(ticks);
    });

    // tas.wait_until(predicate_function)
    tas["wait_until"] = sol::yielding([scheduler](const sol::function &predicate) {
        if (!predicate.valid()) {
            throw sol::error("wait_until: predicate function is invalid");
        }
        scheduler->YieldUntil(predicate);
    });

    // tas.get_tick()
    tas["get_tick"] = [engine]() -> unsigned int {
        try {
            return engine->GetGameInterface()->GetCurrentTick();
        } catch (const std::exception &) {
            return 0;
        }
    };

    // tas.get_manifest()
    tas["get_manifest"] = [engine]() -> sol::table {
        try {
            TASProject *project = engine->GetProjectManager()->GetCurrentProject();
            if (project) {
                return project->GetManifestTable();
            }
        } catch (const std::exception &) {
            // Fall through to return empty table
        }
        return engine->GetLuaState().create_table();
    };
}

// ===================================================================
//  Section 3: Input API Registration
// ===================================================================

void LuaApi::RegisterInputApi(sol::table &tas, TASEngine *engine) {
    auto *inputSystem = engine->GetInputSystem();
    auto *scheduler = engine->GetScheduler();

    // tas.press(key_string)
    tas["press"] = [inputSystem](const std::string &keyString) {
        if (keyString.empty()) {
            throw sol::error("press: key string cannot be empty");
        }

        // Press keys for exactly one frame
        inputSystem->PressKeysOneFrame(keyString);
    };

    // tas.hold(key_string, duration_ticks)
    tas["hold"] = sol::yielding([inputSystem, scheduler](const std::string &keyString, int duration) {
        if (keyString.empty()) {
            throw sol::error("hold: key string cannot be empty");
        }
        if (duration <= 0) {
            throw sol::error("hold: duration must be positive");
        }

        // InputSystem handles the timing internally
        inputSystem->HoldKeys(keyString, duration);

        // Yield for the specified duration
        scheduler->YieldTicks(duration);
    });

    // tas.key_down(key_string)
    tas["key_down"] = [inputSystem](const std::string &keyString) {
        if (keyString.empty()) {
            throw sol::error("key_down: key string cannot be empty");
        }
        inputSystem->PressKeys(keyString);
    };

    // tas.key_up(key_string)
    tas["key_up"] = [inputSystem](const std::string &keyString) {
        if (keyString.empty()) {
            throw sol::error("key_up: key string cannot be empty");
        }
        inputSystem->ReleaseKeys(keyString);
    };

    // tas.release_all_keys()
    tas["release_all_keys"] = [inputSystem]() {
        inputSystem->ReleaseAllKeys();
    };

    // tas.is_key_pressed(key_string)
    tas["is_key_pressed"] = [inputSystem](const std::string &keyString) -> bool {
        if (keyString.empty()) {
            return false;
        }
        return inputSystem->AreKeysPressed(keyString);
    };
}

// ===================================================================
//  Section 4: World Query API Registration
// ===================================================================

void LuaApi::RegisterWorldQueryApi(sol::table &tas, TASEngine *engine) {
    // tas.get_ball()
    tas["get_ball"] = [engine]() -> sol::object {
        try {
            auto *reader = engine->GetGameInterface();
            CK3dEntity *ball = reader->GetBall();
            if (ball) {
                return sol::make_object(engine->GetLuaState(), ball);
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_object(name)
    tas["get_object"] = [engine](const std::string &name) -> sol::object {
        if (name.empty()) {
            return sol::nil;
        }
        try {
            auto *reader = engine->GetGameInterface();
            CK3dEntity *obj = reader->GetObjectByName(name);
            if (obj) {
                return sol::make_object(engine->GetLuaState(), obj);
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_ball_position()
    tas["get_ball_position"] = [engine]() -> sol::object {
        try {
            auto *reader = engine->GetGameInterface();
            CK3dEntity *ball = reader->GetBall();
            if (ball) {
                return sol::make_object(engine->GetLuaState(), reader->GetPosition(ball));
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_ball_velocity()
    tas["get_ball_velocity"] = [engine]() -> sol::object {
        try {
            auto *reader = engine->GetGameInterface();
            CK3dEntity *ball = reader->GetBall();
            if (ball) {
                return sol::make_object(engine->GetLuaState(), reader->GetVelocity(ball));
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };
}

// ===================================================================
//  Section 5: Concurrency & Events API Registration
// ===================================================================

void LuaApi::RegisterConcurrencyApi(sol::table &tas, TASEngine *engine) {
    auto *scheduler = engine->GetScheduler();
    auto *eventManager = engine->GetEventManager();

    // tas.wait(condition_function)
    tas["wait"] = sol::yielding([scheduler](const sol::object &arg) {
        if (!arg.valid()) {
            throw sol::error("wait: argument is invalid");
        }

        std::vector<sol::coroutine> coroutinesToWaitOn;

        if (arg.is<sol::function>()) {
            // Function (condition) - wait until it returns true
            sol::function predicate = arg.as<sol::function>();
            if (!predicate.valid()) {
                throw sol::error("wait: predicate function is invalid");
            }
            scheduler->YieldUntil(predicate);
        }
    });

    // tas.on(event_name, callback_function)
    tas["on"] = [eventManager](const std::string &event_name, const sol::function &callback) {
        if (event_name.empty()) {
            throw sol::error("on: event name cannot be empty");
        }
        if (!callback.valid()) {
            throw sol::error("on: callback function is invalid");
        }

        eventManager->RegisterListener(event_name, callback);
    };
}

// ===================================================================
//  Section 6: Debug & Assertions API Registration
// ===================================================================

void LuaApi::RegisterDebugApi(sol::table &tas, TASEngine *engine) {
    // tas.assert(condition, message)
    tas["assert"] = [](bool condition, const sol::optional<std::string> &message) {
        if (condition) return;

        std::string error_message = message.value_or("Assertion failed!");
        throw sol::error(error_message);
    };
}

// ===================================================================
//  Section 7: Developer Tools API Registration (Gated)
// ===================================================================

void LuaApi::RegisterDevTools(sol::table &tas, TASEngine *engine) {
    auto *dev_tools = engine->GetDevTools();

    if (!dev_tools || !dev_tools->IsEnabled()) {
        tas["tools"] = sol::nil;
        return;
    }

    engine->GetMod()->GetLogger()->Warn("[TAS] Developer mode is enabled. tas.tools API is available.");

    sol::table tools = engine->GetLuaState().create_table();
    tas["tools"] = tools;

    // tas.tools.set_timescale(factor)
    tools["set_timescale"] = [dev_tools](float factor) {
        if (factor <= 0.0f) {
            throw sol::error("set_timescale: factor must be positive");
        }
        if (factor > 10.0f) {
            throw sol::error("set_timescale: factor cannot exceed 10.0");
        }
        dev_tools->SetTimeScale(factor);
    };

    // tas.tools.set_velocity(game_object, velocity_vec3)
    tools["set_velocity"] = [dev_tools](CK3dEntity *obj, const VxVector &vel) {
        if (!obj) {
            throw sol::error("set_velocity: game object is null");
        }
        dev_tools->SetVelocity(obj, vel);
    };

    tools["skip_rendering"] = [dev_tools]() {
        dev_tools->SkipRenderForNextTick();
    };
}
