#include "LuaApi.h"

#include <sstream>
#include <iomanip>

#include <fmt/format.h>
#include <fmt/args.h>

#include "TASEngine.h"
#include "LuaScheduler.h"
#include "InputSystem.h"
#include "EventManager.h"
#include "GameInterface.h"
#include "ProjectManager.h"
#include "TASProject.h"

// Helper function to format strings with variadic arguments
std::string FormatString(const std::string &fmt, sol::variadic_args va) {
    fmt::dynamic_format_arg_store<fmt::format_context> store;

    for (const auto &arg : va) {
        sol::type argType = arg.get_type();
        switch (argType) {
            case sol::type::lua_nil: {
                store.push_back("nil");
                break;
            }
            case sol::type::boolean: {
                bool value = arg.as<bool>();
                store.push_back(value ? "true" : "false");
                break;
            }
            case sol::type::number: {
                // Check if it's an integer or floating point
                if (arg.is<int>()) {
                    store.push_back(arg.as<int>());
                } else if (arg.is<long long>()) {
                    store.push_back(arg.as<long long>());
                } else if (arg.is<float>()) {
                    store.push_back(arg.as<float>());
                } else if (arg.is<double>()) {
                    store.push_back(arg.as<double>());
                } else {
                    // Fallback to double
                    store.push_back(arg.as<double>());
                }
                break;
            }
            case sol::type::string: {
                store.push_back(arg.as<std::string>());
                break;
            }
            case sol::type::table: {
                sol::table table = arg.as<sol::table>();

                // Check if table has a __tostring metamethod
                sol::optional<sol::function> tostring = table[sol::meta_function::to_string];
                if (tostring) {
                    try {
                        sol::protected_function_result result = tostring.value()(table);
                        if (result.valid()) {
                            store.push_back(result.get<std::string>());
                            break;
                        }
                    } catch (const std::exception &) {
                        // Fall through to default table representation
                    }
                }

                // Default table representation
                std::stringstream stream;
                stream << "<table:" << static_cast<void*>(&table) << ">";
                store.push_back(stream.str());
                break;
            }
            case sol::type::userdata: {
                sol::userdata ud = arg.as<sol::userdata>();

                // Check if userdata has a __tostring metamethod
                sol::optional<sol::function> tostring = ud[sol::meta_function::to_string];
                if (tostring) {
                    try {
                        sol::protected_function_result result = tostring.value()(ud);
                        if (result.valid()) {
                            store.push_back(result.get<std::string>());
                            break;
                        }
                    } catch (const std::exception &) {
                        // Fall through to default userdata representation
                    }
                }

                std::stringstream stream;
                stream << "<userdata:" << static_cast<void*>(&ud) << ">";
                store.push_back(stream.str());
                break;
            }
            case sol::type::function: {
                sol::function func = arg.as<sol::function>();
                std::stringstream stream;
                stream << "<function:" << static_cast<void*>(&func) << ">";
                store.push_back(stream.str());
                break;
            }
            case sol::type::thread: {
                sol::thread thread = arg.as<sol::thread>();
                std::stringstream stream;
                stream << "<thread:" << static_cast<void*>(&thread) << ">";
                store.push_back(stream.str());
                break;
            }
            case sol::type::lightuserdata: {
                void* ptr = arg.as<void*>();
                std::stringstream stream;
                stream << "<lightuserdata:" << ptr << ">";
                store.push_back(stream.str());
                break;
            }
            default: {
                // Handle any other types
                std::stringstream stream;
                stream << "<unknown:" << static_cast<int>(argType) << ">";
                store.push_back(stream.str());
                break;
            }
        }
    }

    try {
        return fmt::vformat(fmt, store);
    } catch (const fmt::format_error &e) {
        // If formatting fails, return an error message
        std::stringstream stream;
        stream << "[Format Error: " << e.what() << "] " << fmt;
        return stream.str();
    }
}

// ===================================================================
//  Section 2: Core & Flow Control API Registration
// ===================================================================

void LuaApi::RegisterCoreApi(sol::table &tas, TASEngine *engine) {
    // tas.log(format_string, ...)
    tas["log"] = [engine](const std::string &fmt, sol::variadic_args va) {
        try {
            std::string text = FormatString(fmt, va);
            engine->GetLogger()->Info("[TAS] %s", text.c_str());
        } catch (const std::exception &e) {
            engine->GetLogger()->Error("[TAS] Error in log: %s", e.what());
        }
    };

    // tas.warn(format_string, ...)
    tas["warn"] = [engine](const std::string &fmt, sol::variadic_args va) {
        try {
            std::string text = FormatString(fmt, va);
            engine->GetLogger()->Warn("[TAS] %s", text.c_str());
        } catch (const std::exception &e) {
            engine->GetLogger()->Error("[TAS] Error in warn: %s", e.what());
        }
    };

    // tas.error(format_string, ...)
    tas["error"] = [engine](const std::string &fmt, sol::variadic_args va) {
        try {
            std::string text = FormatString(fmt, va);
            engine->GetLogger()->Error("[TAS] %s", text.c_str());
        } catch (const std::exception &e) {
            engine->GetLogger()->Error("[TAS] Error in error: %s", e.what());
        }
    };

    // tas.print(format_string, ...)
    tas["print"] = [engine](const std::string &fmt, sol::variadic_args va) {
        try {
            std::string text = FormatString(fmt, va);
            engine->GetGameInterface()->PrintMessage(text.c_str());
        } catch (const std::exception &e) {
            engine->GetLogger()->Error("[TAS] Error in print: %s", e.what());
        }
    };

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

    // tas.are_keys_down(key_string)
    tas["are_keys_down"] = [inputSystem](const std::string &keyString) {
        if (keyString.empty()) {
            return false;
        }

        return inputSystem->AreKeysDown(keyString);
    };

    // tas.are_keys_up(key_string)
    tas["are_keys_up"] = [inputSystem](const std::string &keyString) {
        if (keyString.empty()) {
            return false;
        }

        return inputSystem->AreKeysUp(keyString);
    };

    // tas.are_keys_toggled(key_string)
    tas["are_keys_toggled"] = [inputSystem](const std::string &keyString) -> bool {
        if (keyString.empty()) {
            return false;
        }
        return inputSystem->AreKeysToggled(keyString);
    };
}

// ===================================================================
//  Section 4: World Query API Registration
// ===================================================================

void LuaApi::RegisterWorldQueryApi(sol::table &tas, TASEngine *engine) {
    // tas.is_legacy_mode()
    tas["is_legacy_mode"] = [engine]() -> bool {
        const auto *g = engine->GetGameInterface();
        if (!g) {
            return false;
        }

        return g->IsLegacyMode();
    };

    // tas.is_paused()
    tas["is_paused"] = [engine]() -> bool {
        const auto *g = engine->GetGameInterface();
        if (!g) {
            return false;
        }

        return g->IsPaused();
    };

    // tas.is_playing()
    tas["is_playing"] = [engine]() -> bool {
        const auto *g = engine->GetGameInterface();
        if (!g) {
            return false;
        }

        return g->IsPlaying();
    };

    // tas.get_sr_score()
    tas["get_sr_score"] = [engine]() -> float {
        const auto *g = engine->GetGameInterface();
        if (!g) {
            return false;
        }

        return g->GetSRScore();
    };

    // tas.get_hs_score()
    tas["get_hs_score"] = [engine]() -> int {
        const auto *g = engine->GetGameInterface();
        if (!g) {
            return false;
        }

        return g->GetHSScore();
    };

    // tas.get_points()
    tas["get_points"] = [engine]() -> int {
        const auto *g = engine->GetGameInterface();
        if (!g) {
            return 0;
        }

        return g->GetPoints();
    };

    // tas.get_life_count()
    tas["get_life_count"] = [engine]() -> int {
        const auto *g = engine->GetGameInterface();
        if (!g) {
            return 0;
        }

        return g->GetLifeCount();
    };

    // tas.get_sector()
    tas["get_sector"] = [engine]() -> int {
        const auto *g = engine->GetGameInterface();
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
            const auto *g = engine->GetGameInterface();
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
            const auto *g = engine->GetGameInterface();
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
            const auto *g = engine->GetGameInterface();
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
            const auto *g = engine->GetGameInterface();
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
            const auto *g = engine->GetGameInterface();
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
            const auto *g = engine->GetGameInterface();
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
            const auto *g = engine->GetGameInterface();
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
            const auto *g = engine->GetGameInterface();
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

    // ===================================================================
    // Basic waiting operations
    // ===================================================================

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

    // ===================================================================
    // Repeat operations
    // ===================================================================

    // tas.repeat_for(task, ticks) - repeat task for N ticks
    tas["repeat_for"] = sol::yielding([scheduler](const sol::function &task, int ticks) {
        if (!task.valid()) {
            throw sol::error("repeat_for: task function is invalid");
        }
        if (ticks <= 0) {
            throw sol::error("repeat_for: tick count must be positive");
        }
        scheduler->YieldRepeatFor(task, ticks);
    });

    // tas.repeat_until(task, condition) - repeat task until condition is true
    tas["repeat_until"] = sol::yielding([scheduler](const sol::function &task, const sol::function &condition) {
        if (!task.valid()) {
            throw sol::error("repeat_until: task function is invalid");
        }
        if (!condition.valid()) {
            throw sol::error("repeat_until: condition function is invalid");
        }
        scheduler->YieldRepeatUntil(task, condition);
    });

    // tas.repeat_while(task, condition) - repeat task while condition is true
    tas["repeat_while"] = sol::yielding([scheduler](const sol::function &task, const sol::function &condition) {
        if (!task.valid()) {
            throw sol::error("repeat_while: task function is invalid");
        }
        if (!condition.valid()) {
            throw sol::error("repeat_while: condition function is invalid");
        }
        scheduler->YieldRepeatWhile(task, condition);
    });

    // tas.repeat_count(task, count) - repeat task N times
    tas["repeat_count"] = sol::yielding([scheduler](const sol::function &task, int count) {
        if (!task.valid()) {
            throw sol::error("repeat_count: task function is invalid");
        }
        if (count <= 0) {
            throw sol::error("repeat_count: count must be positive");
        }
        scheduler->YieldRepeatCount(task, count);
    });

    // ===================================================================
    // Timing and delay operations
    // ===================================================================

    // tas.delay(task, ticks) - delay task execution by N ticks
    tas["delay"] = sol::yielding([scheduler](const sol::function &task, int delay_ticks) {
        if (!task.valid()) {
            throw sol::error("delay: task function is invalid");
        }
        if (delay_ticks < 0) {
            throw sol::error("delay: delay ticks cannot be negative");
        }
        scheduler->YieldDelay(task, delay_ticks);
    });

    // tas.timeout(task, ticks) - run task with timeout
    tas["timeout"] = sol::yielding([scheduler](const sol::function &task, int timeout_ticks) {
        if (!task.valid()) {
            throw sol::error("timeout: task function is invalid");
        }
        if (timeout_ticks <= 0) {
            throw sol::error("timeout: timeout must be positive");
        }
        scheduler->YieldTimeout(task, timeout_ticks);
    });

    // tas.debounce(task, ticks) - debounce task execution
    tas["debounce"] = sol::yielding([scheduler](const sol::function &task, int debounce_ticks) {
        if (!task.valid()) {
            throw sol::error("debounce: task function is invalid");
        }
        if (debounce_ticks <= 0) {
            throw sol::error("debounce: debounce ticks must be positive");
        }
        scheduler->YieldDebounce(task, debounce_ticks);
    });

    // ===================================================================
    // Control flow operations
    // ===================================================================

    // tas.sequence(tasks...) - run tasks in sequence
    tas["sequence"] = sol::yielding([scheduler](sol::variadic_args va) {
        std::vector<sol::function> tasks;
        for (const auto &arg : va) {
            if (arg.is<sol::function>()) {
                tasks.push_back(arg.as<sol::function>());
            } else {
                throw sol::error("sequence: all arguments must be functions");
            }
        }
        if (tasks.empty()) {
            throw sol::error("sequence: no tasks provided");
        }
        scheduler->YieldSequence(tasks);
    });

    // tas.retry(task, max_attempts) - retry task up to N times
    tas["retry"] = sol::yielding([scheduler](const sol::function &task, int max_attempts) {
        if (!task.valid()) {
            throw sol::error("retry: task function is invalid");
        }
        if (max_attempts <= 0) {
            throw sol::error("retry: max attempts must be positive");
        }
        scheduler->YieldRetry(task, max_attempts);
    });

    // ===================================================================
    // Parallel operations
    // ===================================================================

    // tas.parallel(coroutines...) - run coroutines in parallel, wait for all
    tas["parallel"] = sol::yielding([scheduler](sol::variadic_args va) {
        std::vector<sol::coroutine> coroutines;
        for (const auto &arg : va) {
            if (arg.is<sol::function>()) {
                // Convert function to coroutine
                sol::function func = arg.as<sol::function>();
                coroutines.emplace_back(func);
            } else if (arg.is<sol::coroutine>()) {
                coroutines.push_back(arg.as<sol::coroutine>());
            } else {
                throw sol::error("parallel: arguments must be functions or coroutines");
            }
        }
        if (coroutines.empty()) {
            throw sol::error("parallel: no coroutines provided");
        }
        scheduler->YieldParallel(coroutines);
    });

    // tas.race(coroutines...) - run coroutines in parallel, complete when first finishes
    tas["race"] = sol::yielding([scheduler](sol::variadic_args va) {
        std::vector<sol::coroutine> coroutines;
        for (const auto &arg : va) {
            if (arg.is<sol::function>()) {
                // Convert function to coroutine
                sol::function func = arg.as<sol::function>();
                coroutines.emplace_back(func);
            } else if (arg.is<sol::coroutine>()) {
                coroutines.push_back(arg.as<sol::coroutine>());
            } else {
                throw sol::error("race: arguments must be functions or coroutines");
            }
        }
        if (coroutines.empty()) {
            throw sol::error("race: no coroutines provided");
        }
        scheduler->YieldRace(coroutines);
    });

    // tas.all(coroutines...) - alias for parallel
    tas["all"] = tas["parallel"];

    // ===================================================================
    // Event-based operations
    // ===================================================================

    // tas.wait_for_event(event_name) - wait for specific event
    tas["wait_for_event"] = sol::yielding([scheduler](const std::string &event_name) {
        if (event_name.empty()) {
            throw sol::error("wait_for_event: event name cannot be empty");
        }
        scheduler->YieldWaitForEvent(event_name);
    });
}

void LuaApi::RegisterEventApi(sol::table &tas, TASEngine *engine) {
    auto *eventManager = engine->GetEventManager();

    // Register event listener from Lua
    tas["on"] = sol::overload(
        [eventManager](const std::string &eventName, const sol::function &callback) {
            if (eventName.empty()) {
                throw sol::error("on: event name cannot be empty");
            }
            if (!callback.valid()) {
                throw sol::error("on: callback function is invalid");
            }
            eventManager->RegisterListener(eventName, callback);
        },
        [eventManager](const std::string &eventName, const sol::function &callback, bool oneTime) {
            if (eventName.empty()) {
                throw sol::error("on: event name cannot be empty");
            }
            if (!callback.valid()) {
                throw sol::error("on: callback function is invalid");
            }
            eventManager->RegisterListener(eventName, callback, oneTime);
        }
    );

    // Register one-time event listener from Lua
    tas["once"] = [eventManager](const std::string &eventName, const sol::function &callback) {
        if (eventName.empty()) {
            throw sol::error("once: event name cannot be empty");
        }
        if (!callback.valid()) {
            throw sol::error("once: callback function is invalid");
        }
        eventManager->RegisterOnceListener(eventName, callback);
    };

    // Fire event from Lua (with up to 3 arguments for simplicity)
    tas["fire_event"] = sol::overload(
        [eventManager](const std::string &eventName) {
            eventManager->FireEvent(eventName);
        },
        [eventManager](const std::string &eventName, const sol::object &arg1) {
            if (arg1.is<int>()) {
                eventManager->FireEvent(eventName, arg1.as<int>());
            } else if (arg1.is<float>()) {
                eventManager->FireEvent(eventName, arg1.as<float>());
            } else if (arg1.is<std::string>()) {
                eventManager->FireEvent(eventName, arg1.as<std::string>());
            } else {
                eventManager->FireEvent(eventName);
            }
        },
        [eventManager](const std::string &eventName, const sol::object &arg1, const sol::object &arg2) {
            // Handle common two-argument cases
            if (arg1.is<std::string>() && arg2.is<int>()) {
                eventManager->FireEvent(eventName, arg1.as<std::string>(), arg2.as<int>());
            } else if (arg1.is<int>() && arg2.is<int>()) {
                eventManager->FireEvent(eventName, arg1.as<int>(), arg2.as<int>());
            } else {
                eventManager->FireEvent(eventName);
            }
        }
    );

    // Event management from Lua
    tas["clear_listeners"] = sol::overload(
        [eventManager]() {
            eventManager->ClearListeners();
        },
        [eventManager](const std::string &eventName) {
            eventManager->ClearListeners(eventName);
        }
    );

    tas["get_listener_count"] = [eventManager](const std::string &eventName) {
        return eventManager->GetListenerCount(eventName);
    };

    tas["has_listeners"] = [eventManager](const std::string &eventName) {
        return eventManager->HasListeners(eventName);
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
