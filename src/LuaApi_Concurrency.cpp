#include "LuaApi.h"

#include <stdexcept>

#include "LuaScheduler.h"
#include "EventManager.h"
#include "ScriptContext.h"

// ===================================================================
//  Concurrency & Events API Registration
// ===================================================================

void LuaApi::RegisterConcurrencyApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterConcurrencyApi requires a valid ScriptContext");
    }

    auto *scheduler = context->GetScheduler();
    if (!scheduler) {
        throw std::runtime_error("LuaApi::RegisterConcurrencyApi: Scheduler not available for this context");
    }

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

void LuaApi::RegisterEventApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterEventApi requires a valid ScriptContext");
    }

    auto *eventManager = context->GetEventManager();
    if (!eventManager) {
        throw std::runtime_error("LuaApi::RegisterEventApi: Event manager not available for this context");
    }

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
