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
    // Fast path for empty format strings
    if (fmt.empty()) {
        return fmt;
    }

    // Fast path for format strings without placeholders
    if (fmt.find('{') == std::string::npos) {
        return fmt;
    }

    fmt::dynamic_format_arg_store<fmt::format_context> store;
    store.reserve(va.size());  // Pre-allocate for better performance

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
                store.push_back(arg.as<double>());
                break;
            }
            case sol::type::string: {
                sol::string_view sv = arg.as<sol::string_view>();
                store.push_back(sv);
                break;
            }
            case sol::type::table: {
                sol::table table = arg.as<sol::table>();
                sol::table metatable = table[sol::metatable_key];

                // Check if table has a __tostring metamethod
                sol::optional<sol::function> tostring = metatable[sol::to_string(sol::meta_function::to_string)];
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

                store.push_back("<table>");
                break;
            }
            case sol::type::userdata: {
                sol::userdata ud = arg.as<sol::userdata>();
                sol::table metatable = ud[sol::metatable_key];

                // Check if userdata has a __tostring metamethod
                auto tostring = metatable[sol::to_string(sol::meta_function::to_string)];
                if (tostring.valid() && tostring.get_type() == sol::type::function) {
                    auto res = tostring.call<sol::protected_function>(ud);
                    if (res.valid() && res.get_type() == sol::type::string) {
                        store.push_back(res.as<std::string>());
                        break;
                    }
                }

                store.push_back("<userdata>");
                break;
            }
            case sol::type::function: {
                store.push_back("<<function>");
                break;
            }
            case sol::type::thread: {
                store.push_back("<thread>");
                break;
            }
            case sol::type::lightuserdata: {
                store.push_back("<lightuserdata>");
                break;
            }
            default: {
                store.push_back("<unknown>");
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
    } catch (const std::exception &e) {
        // Catch any other standard exceptions
        std::stringstream stream;
        stream << "[Exception: " << e.what() << "] " << fmt;
        return stream.str();
    } catch (...) {
        // Catch any unknown exceptions
        std::stringstream stream;
        stream << "[Unknown Error] " << fmt;
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
            return 0.0f;
        }

        return g->GetSRScore();
    };

    // tas.get_hs_score()
    tas["get_hs_score"] = [engine]() -> int {
        const auto *g = engine->GetGameInterface();
        if (!g) {
            return 0;
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

    // tas.get_level()
    tas["get_level"] = [engine]() -> int {
        const auto *g = engine->GetGameInterface();
        if (!g) {
            return 0;
        }

        return g->GetCurrentLevel();
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
                CK3dEntity *obj = g->GetObjectByName(name);
                if (obj) {
                    return sol::make_object(engine->GetLuaState(), obj);
                }
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
            if (!g) {
                return sol::nil;
            }
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
            if (!g) {
                return sol::nil;
            }
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
    // Basic waiting operations (YIELDING)
    // ===================================================================

    // tas.wait_ticks(num_ticks) - pause for N ticks
    tas["wait_ticks"] = sol::yielding([scheduler](int ticks) {
        if (ticks <= 0) {
            throw sol::error("wait_ticks: tick count must be positive");
        }
        scheduler->YieldTicks(ticks);
    });

    // tas.wait_event(event_name) - wait for specific event
    tas["wait_event"] = sol::yielding([scheduler](const std::string &eventName) {
        if (eventName.empty()) {
            throw sol::error("wait_event: event name cannot be empty");
        }
        scheduler->YieldWaitForEvent(eventName);
    });

    // tas.wait_until(predicate_function) - wait until condition is true
    tas["wait_until"] = sol::yielding([scheduler](sol::function predicate) {
        if (!predicate.valid()) {
            throw sol::error("wait_until: predicate function is invalid");
        }
        scheduler->YieldUntil(predicate);
    });

    // tas.wait_coroutines(coroutines...) - wait for all coroutines to complete
    tas["wait_coroutines"] = sol::yielding([scheduler](sol::variadic_args va) {
        std::vector<sol::coroutine> coroutines;
        for (const auto &arg : va) {
            if (arg.is<sol::coroutine>()) {
                coroutines.push_back(arg.as<sol::coroutine>());
            } else {
                throw sol::error("wait_coroutines: all arguments must be coroutines");
            }
        }
        if (coroutines.empty()) {
            throw sol::error("wait_coroutines: no coroutines provided");
        }
        scheduler->YieldCoroutines(coroutines);
    });

    // tas.wait(condition) - flexible wait function
    tas["wait"] = sol::yielding([scheduler](sol::object arg) {
        if (!arg.valid()) {
            throw sol::error("wait: argument is invalid");
        }

        if (arg.is<int>()) {
            // wait(5) - wait N ticks
            int ticks = arg.as<int>();
            if (ticks <= 0) {
                throw sol::error("wait: tick count must be positive");
            }
            scheduler->YieldTicks(ticks);
        } else if (arg.is<std::string>()) {
            // wait("event") - wait for event
            std::string eventName = arg.as<std::string>();
            if (eventName.empty()) {
                throw sol::error("wait: event name cannot be empty");
            }
            scheduler->YieldWaitForEvent(eventName);
        } else if (arg.is<sol::function>()) {
            // wait(function) - wait until predicate true
            sol::function predicate = arg.as<sol::function>();
            if (!predicate.valid()) {
                throw sol::error("wait: predicate function is invalid");
            }
            scheduler->YieldUntil(predicate);
        } else if (arg.is<sol::table>()) {
            // wait({co1, co2}) - wait for coroutines
            sol::table t = arg.as<sol::table>();
            std::vector<sol::coroutine> coroutines;
            for (auto &pair : t) {
                if (pair.second.is<sol::coroutine>()) {
                    coroutines.push_back(pair.second.as<sol::coroutine>());
                }
            }
            if (coroutines.empty()) {
                throw sol::error("wait: no valid coroutines found in table");
            }
            scheduler->YieldCoroutines(coroutines);
        } else {
            throw sol::error("wait: unsupported argument type (expected number, string, function, or table)");
        }
    });

    // ===================================================================
    // Coordination operations (YIELDING) - wait for completion
    // ===================================================================

    // tas.parallel(tasks...) - run tasks in parallel and wait for ALL to complete
    tas["parallel"] = sol::yielding([scheduler](sol::variadic_args va) {
        std::vector<sol::coroutine> coroutines;

        for (const auto &arg : va) {
            if (arg.is<sol::function>()) {
                // PREFERRED: Start coroutine from function (always safe)
                sol::function func = arg.as<sol::function>();
                if (!func.valid()) {
                    throw sol::error("parallel: invalid function provided");
                }
                sol::coroutine co = scheduler->StartCoroutineAndTrack(func);
                if (co.valid()) {
                    coroutines.push_back(co);
                }
            } else if (arg.is<sol::coroutine>()) {
                // CAREFUL: Handle existing coroutines safely
                sol::coroutine co = arg.as<sol::coroutine>();
                if (!co.valid()) {
                    throw sol::error("parallel: invalid coroutine provided");
                }
                sol::coroutine tracked_co = scheduler->StartCoroutineAndTrack(co);
                if (tracked_co.valid()) {
                    coroutines.push_back(tracked_co);
                }
            } else {
                throw sol::error("parallel: arguments must be functions or coroutines");
            }
        }

        if (coroutines.empty()) {
            throw sol::error("parallel: no valid tasks provided");
        }

        // Wait for all coroutines to complete
        scheduler->YieldCoroutines(coroutines);
    });

    // tas.race(tasks...) - run tasks in parallel and wait for FIRST to complete
    tas["race"] = sol::yielding([scheduler](sol::variadic_args va) {
        std::vector<sol::coroutine> coroutines;

        for (const auto &arg : va) {
            if (arg.is<sol::function>()) {
                // PREFERRED: Start coroutine from function (always safe)
                sol::function func = arg.as<sol::function>();
                if (!func.valid()) {
                    throw sol::error("race: invalid function provided");
                }
                sol::coroutine co = scheduler->StartCoroutineAndTrack(func);
                if (co.valid()) {
                    coroutines.push_back(co);
                }
            } else if (arg.is<sol::coroutine>()) {
                // CAREFUL: Handle existing coroutines safely
                sol::coroutine co = arg.as<sol::coroutine>();
                if (!co.valid()) {
                    throw sol::error("race: invalid coroutine provided");
                }
                sol::coroutine tracked_co = scheduler->StartCoroutineAndTrack(co);
                if (tracked_co.valid()) {
                    coroutines.push_back(tracked_co);
                }
            } else {
                throw sol::error("race: arguments must be functions or coroutines");
            }
        }

        if (coroutines.empty()) {
            throw sol::error("race: no valid tasks provided");
        }

        // Wait for first coroutine to complete
        scheduler->YieldRace(coroutines);
    });

    // ===================================================================
    // Background operations (NON-YIELDING) - fire and forget
    // ===================================================================

    // tas.spawn(tasks...) - start tasks in background, continue immediately
    tas["spawn"] = [scheduler](sol::variadic_args va) {
        for (const auto &arg : va) {
            if (arg.is<sol::function>()) {
                sol::function func = arg.as<sol::function>();
                if (func.valid()) {
                    scheduler->AddCoroutineTask(func);
                }
            } else if (arg.is<sol::coroutine>()) {
                sol::coroutine co = arg.as<sol::coroutine>();
                if (co.valid()) {
                    scheduler->AddCoroutineTask(co);
                }
            } else {
                throw sol::error("spawn: arguments must be functions or coroutines");
            }
        }
    };

    // ===================================================================
    // Background repeat operations (NON-YIELDING)
    // ===================================================================

    // tas.repeat_ticks(task, ticks) - repeat task for N ticks in background
    tas["repeat_ticks"] = [scheduler](sol::function task, int ticks) {
        if (!task.valid()) {
            throw sol::error("repeat_ticks: invalid task function");
        }
        if (ticks <= 0) {
            throw sol::error("repeat_ticks: tick count must be positive");
        }
        scheduler->StartRepeatFor(task, ticks);
    };

    // tas.repeat_until(task, condition) - repeat task until condition is true in background
    tas["repeat_until"] = [scheduler](sol::function task, sol::function condition) {
        if (!task.valid()) {
            throw sol::error("repeat_until: invalid task function");
        }
        if (!condition.valid()) {
            throw sol::error("repeat_until: invalid condition function");
        }
        scheduler->StartRepeatUntil(task, condition);
    };

    // tas.repeat_while(task, condition) - repeat task while condition is true in background
    tas["repeat_while"] = [scheduler](sol::function task, sol::function condition) {
        if (!task.valid()) {
            throw sol::error("repeat_while: invalid task function");
        }
        if (!condition.valid()) {
            throw sol::error("repeat_while: invalid condition function");
        }
        scheduler->StartRepeatWhile(task, condition);
    };

    // ===================================================================
    // Background timing operations (NON-YIELDING)
    // ===================================================================

    // tas.delay(task, ticks) - delay task execution by N ticks in background
    tas["delay"] = [scheduler](sol::function task, int delay_ticks) {
        if (!task.valid()) {
            throw sol::error("delay: invalid task function");
        }
        if (delay_ticks < 0) {
            throw sol::error("delay: delay ticks cannot be negative");
        }
        scheduler->StartDelay(task, delay_ticks);
    };

    // tas.timeout(task, ticks) - run task with timeout in background
    tas["timeout"] = [scheduler](sol::function task, int timeoutTicks) {
        if (!task.valid()) {
            throw sol::error("timeout: invalid task function");
        }
        if (timeoutTicks <= 0) {
            throw sol::error("timeout: timeout must be positive");
        }
        scheduler->StartTimeout(task, timeoutTicks);
    };

    // tas.debounce(task, ticks) - debounce task execution in background
    tas["debounce"] = [scheduler](sol::function task, int debounceTicks) {
        if (!task.valid()) {
            throw sol::error("debounce: invalid task function");
        }
        if (debounceTicks <= 0) {
            throw sol::error("debounce: debounce ticks must be positive");
        }
        scheduler->StartDebounce(task, debounceTicks);
    };

    // ===================================================================
    // Background control flow operations (NON-YIELDING)
    // ===================================================================

    // tas.sequence(tasks...) - run tasks in sequence in background
    tas["sequence"] = [scheduler](sol::variadic_args va) {
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
        scheduler->StartSequence(tasks);
    };

    // tas.retry(task, max_attempts) - retry task up to N times in background
    tas["retry"] = [scheduler](sol::function task, int maxAttempts) {
        if (!task.valid()) {
            throw sol::error("retry: invalid task function");
        }
        if (maxAttempts <= 0) {
            throw sol::error("retry: max attempts must be positive");
        }
        scheduler->StartRetry(task, maxAttempts);
    };
}

void LuaApi::RegisterEventApi(sol::table &tas, TASEngine *engine) {
    auto *eventManager = engine->GetEventManager();

    // Register event listener from Lua
    tas["on"] = sol::overload(
        [eventManager](const std::string &eventName, sol::function callback) {
            if (eventName.empty()) {
                throw sol::error("on: event name cannot be empty");
            }
            if (!callback.valid()) {
                throw sol::error("on: callback function is invalid");
            }
            eventManager->RegisterListener(eventName, callback);
        },
        [eventManager](const std::string &eventName, sol::function callback, bool oneTime) {
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
    tas["once"] = [eventManager](const std::string &eventName, sol::function callback) {
        if (eventName.empty()) {
            throw sol::error("once: event name cannot be empty");
        }
        if (!callback.valid()) {
            throw sol::error("once: callback function is invalid");
        }
        eventManager->RegisterOnceListener(eventName, callback);
    };

    // Fire event from Lua
    tas["send"] = sol::overload(
        [eventManager](const std::string &eventName) {
            if (eventName.empty()) {
                throw sol::error("send: event name cannot be empty");
            }
            eventManager->FireEvent(eventName);
        },
        [eventManager](const std::string &eventName, sol::variadic_args va) {
            if (eventName.empty()) {
                throw sol::error("send: event name cannot be empty");
            }
            eventManager->FireEvent(eventName, va);
        }
    );

    // Event management from Lua
    tas["clear_listeners"] = sol::overload(
        [eventManager]() {
            eventManager->ClearListeners();
        },
        [eventManager](const std::string &eventName) {
            if (eventName.empty()) {
                throw sol::error("clear_listeners: event name cannot be empty");
            }
            eventManager->ClearListeners(eventName);
        }
    );

    tas["get_listener_count"] = [eventManager](const std::string &eventName) {
        if (eventName.empty()) {
            throw sol::error("get_listener_count: event name cannot be empty");
        }
        return eventManager->GetListenerCount(eventName);
    };

    tas["has_listeners"] = [eventManager](const std::string &eventName) {
        if (eventName.empty()) {
            throw sol::error("has_listeners: event name cannot be empty");
        }
        return eventManager->HasListeners(eventName);
    };
}

// ===================================================================
//  Section 6: Debug & Assertions API Registration
// ===================================================================

void LuaApi::RegisterDebugApi(sol::table &tas, TASEngine *engine) {
    // tas.assert(condition, message)
    tas["assert"] = [](bool condition, sol::optional<std::string> message) {
        if (condition) return;

        std::string error_message = message.value_or("Assertion failed!");
        throw sol::error(error_message);
    };

    // tas.skip_rendering(ticks)
    tas["skip_rendering"] = [engine](size_t ticks) {
        auto *g = engine->GetGameInterface();
        if (g) {
            g->SkipRenderForTicks(ticks);
        }
    };
}
