#include "LuaApi.h"

#include <sstream>
#include <iomanip>

#include "TASEngine.h"
#include "LuaScheduler.h"
#include "InputSystem.h"
#include "EventManager.h"
#include "GameInterface.h"
#include "ProjectManager.h"
#include "TASProject.h"

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

    // tas.log(format_string, ...)
    tas["log"] = [engine](const std::string &fmt, const sol::variadic_args &va) {
        try {
            std::string text = FormatString(fmt, va);
            engine->GetLogger()->Info("[TAS] %s", text.c_str());
        } catch (const std::exception &e) {
            engine->GetLogger()->Error("[TAS] Error in log: %s", e.what());
        }
    };

    // tas.warn(format_string, ...)
    tas["warn"] = [engine](const std::string &fmt, const sol::variadic_args &va) {
        try {
            std::string text = FormatString(fmt, va);
            engine->GetLogger()->Warn("[TAS] %s", text.c_str());
        } catch (const std::exception &e) {
            engine->GetLogger()->Error("[TAS] Error in warn: %s", e.what());
        }
    };

    // tas.error(format_string, ...)
    tas["error"] = [engine](const std::string &fmt, const sol::variadic_args &va) {
        try {
            std::string text = FormatString(fmt, va);
            engine->GetLogger()->Error("[TAS] %s", text.c_str());
        } catch (const std::exception &e) {
            engine->GetLogger()->Error("[TAS] Error in error: %s", e.what());
        }
    };

    // tas.print(format_string, ...)
    tas["print"] = [engine](const std::string &fmt, const sol::variadic_args &va) {
        try {
            std::string text = FormatString(fmt, va);
            engine->GetGameInterface()->PrintMessage(text.c_str());
        } catch (const std::exception &e) {
            engine->GetLogger()->Error("[TAS] Error in print: %s", e.what());
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
        return engine->GetCurrentTick();
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
    // tas.is_legacy_mode()
    tas["is_legacy_mode"] = [engine]() -> bool {
        return engine->GetGameInterface()->IsLegacyMode();
    };

    // tas.is_paused()
    tas["is_paused"] = [engine]() -> bool {
        auto *g = engine->GetGameInterface();
        if (!g) {
            return false;
        }

        return g->IsPaused();
    };

    // tas.is_playing()
    tas["is_playing"] = [engine]() -> bool {
        auto *g = engine->GetGameInterface();
        if (!g) {
            return false;
        }

        return g->IsPlaying();
    };

    // tas.get_sr_score()
    tas["get_sr_score"] = [engine]() -> float {
        auto *g = engine->GetGameInterface();
        if (!g) {
            return false;
        }

        return g->GetSRScore();
    };

    // tas.get_hs_score()
    tas["get_hs_score"] = [engine]() -> int {
        auto *g = engine->GetGameInterface();
        if (!g) {
            return false;
        }

        return g->GetHSScore();
    };

    // tas.get_points()
    tas["get_points"] = [engine]() -> int {
        auto *g = engine->GetGameInterface();
        if (!g) {
            return 0;
        }

        return g->GetPoints();
    };

    // tas.get_life_count()
    tas["get_life_count"] = [engine]() -> int {
        auto *g = engine->GetGameInterface();
        if (!g) {
            return 0;
        }

        return g->GetLifeCount();
    };

    // tas.get_sector()
    tas["get_sector"] = [engine]() -> int {
        auto *g = engine->GetGameInterface();
        if (!g) {
            return 0;
        }

        return g->GetCurrentSector();
    };

    // tas.get_object(name)
    tas["get_object"] = [engine](const std::string &name) -> sol::object {
        if (name.empty()) {
            return sol::nil;
        }
        try {
            auto *g = engine->GetGameInterface();
            if (g) {
                return sol::make_object(engine->GetLuaState(), g);
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_object_by_id(id)
    tas["get_object_by_id"] = [engine](int id) -> sol::object {
        if (id <= 0) {
            return sol::nil;
        }
        try {
            auto *g = engine->GetGameInterface();
            CK3dEntity *obj = g->GetObjectByID(id);
            if (obj) {
                return sol::make_object(engine->GetLuaState(), obj);
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_physics_object(entity)
    tas["get_physics_object"] = [engine](CK3dEntity *entity) -> sol::object {
        if (!entity) {
            return sol::nil;
        }
        try {
            auto *g = engine->GetGameInterface();
            PhysicsObject *obj = g->GetPhysicsObject(entity);
            if (obj) {
                return sol::make_object(engine->GetLuaState(), obj);
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_ball()
    tas["get_ball"] = [engine]() -> sol::object {
        try {
            auto *g = engine->GetGameInterface();
            CK3dEntity *ball = g->GetActiveBall();
            if (ball) {
                return sol::make_object(engine->GetLuaState(), ball);
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_ball_position()
    tas["get_ball_position"] = [engine]() -> sol::object {
        try {
            auto *g = engine->GetGameInterface();
            CK3dEntity *ball = g->GetActiveBall();
            if (ball) {
                return sol::make_object(engine->GetLuaState(), g->GetPosition(ball));
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_ball_velocity()
    tas["get_ball_velocity"] = [engine]() -> sol::object {
        try {
            auto *g = engine->GetGameInterface();
            CK3dEntity *ball = g->GetActiveBall();
            if (ball) {
                return sol::make_object(engine->GetLuaState(), g->GetVelocity(ball));
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_ball_angular_velocity()
    tas["get_ball_angular_velocity"] = [engine]() -> sol::object {
        try {
            auto *g = engine->GetGameInterface();
            CK3dEntity *ball = g->GetActiveBall();
            if (ball) {
                return sol::make_object(engine->GetLuaState(), g->GetAngularVelocity(ball));
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_camera()
    tas["get_camera"] = [engine]() -> sol::object {
        try {
            auto *g = engine->GetGameInterface();
            CK3dEntity *camera = g->GetActiveCamera();
            if (camera) {
                return sol::make_object(engine->GetLuaState(), camera);
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

    tas["skip_rendering"] = [engine]() {
        auto *g = engine->GetGameInterface();
        if (g) {
            g->SkipRenderForNextTick();
        }
    };
}