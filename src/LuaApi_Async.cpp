#include "LuaApi.h"

#include "Logger.h"
#include "TASEngine.h"
#include "ScriptContext.h"
#include "LuaScheduler.h"
#include "AsyncTask.h"
#include "EventManager.h"

// ===================================================================
//  Async/Await API Registration
// ===================================================================

void LuaApi::RegisterAsyncApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterAsyncApi requires a valid ScriptContext");
    }

    std::string logPrefix = "[" + context->GetName() + "]";
    sol::state &lua = context->GetLuaState();
    LuaScheduler *scheduler = context->GetScheduler();

    // Register AsyncTask userdata type
    sol::usertype<AsyncTask> task_type = lua.new_usertype<AsyncTask>(
        "AsyncTask",
        sol::no_constructor
    );

    // --- State Checking Methods ---

    task_type["is_pending"] = &AsyncTask::IsPending;
    task_type["is_running"] = &AsyncTask::IsRunning;
    task_type["is_completed"] = &AsyncTask::IsCompleted;
    task_type["is_failed"] = &AsyncTask::IsFailed;
    task_type["is_cancelled"] = &AsyncTask::IsCancelled;
    task_type["is_done"] = &AsyncTask::IsDone;

    // --- Result Access ---

    task_type["get_result"] = &AsyncTask::GetResult;
    task_type["get_error"] = &AsyncTask::GetError;

    // --- Control Methods ---

    task_type["start"] = &AsyncTask::Start;
    task_type["cancel"] = &AsyncTask::Cancel;

    // task:await() - Block until task completes
    task_type["await"] = [scheduler](AsyncTask &self) -> sol::object {
        // Start task if not started
        if (self.IsPending()) {
            self.Start();
        }

        // Poll until done
        while (!self.IsDone()) {
            // Yield to scheduler to let task progress
            scheduler->YieldTicks(1);
        }

        if (self.IsCompleted()) {
            return self.GetResult();
        } else if (self.IsFailed()) {
            throw sol::error("AsyncTask failed: " + self.GetError());
        } else {
            throw sol::error("AsyncTask was cancelled");
        }
    };

    // task:then(fn) - Chain operation (Promise-like)
    task_type["then"] = [scheduler, context](AsyncTask &self, sol::function fn) -> std::shared_ptr<AsyncTask> {
        sol::state &lua = context->GetLuaState();

        // Create wrapper function that creates a coroutine with bound arguments
        sol::protected_function_result loaded = lua.safe_script(R"(
            return function(task, fn, scheduler)
                return coroutine.create(function()
                    while not task:is_done() do
                        scheduler:YieldTicks(1)
                    end
                    if task:is_completed() then
                        return fn(task:get_result())
                    elseif task:is_failed() then
                        error("Chained task failed: " .. task:get_error())
                    else
                        error("Chained task was cancelled")
                    end
                end)
            end
        )");

        if (!loaded.valid()) {
            throw sol::error("Failed to create then() wrapper");
        }

        sol::function wrapper = loaded;
        sol::protected_function_result co_result = wrapper(&self, fn, scheduler);

        if (!co_result.valid()) {
            throw sol::error("Failed to create then() coroutine");
        }

        sol::coroutine chain_co = co_result;

        auto chained_task = std::make_shared<AsyncTask>(scheduler, chain_co, context);
        chained_task->Start();
        return chained_task;
    };

    // task:catch(fn) - Error handler (Promise-like)
    task_type["catch"] = [scheduler, context](AsyncTask &self, sol::function fn) -> std::shared_ptr<AsyncTask> {
        sol::state &lua = context->GetLuaState();

        // Create wrapper function that creates a coroutine with bound arguments
        sol::protected_function_result loaded = lua.safe_script(R"(
            return function(task, fn, scheduler)
                return coroutine.create(function()
                    while not task:is_done() do
                        scheduler:YieldTicks(1)
                    end
                    if task:is_completed() then
                        return task:get_result()
                    else
                        local error = task:is_failed() and task:get_error() or "Task was cancelled"
                        return fn(error)
                    end
                end)
            end
        )");

        if (!loaded.valid()) {
            throw sol::error("Failed to create catch() wrapper");
        }

        sol::function wrapper = loaded;
        sol::protected_function_result co_result = wrapper(&self, fn, scheduler);

        if (!co_result.valid()) {
            throw sol::error("Failed to create catch() coroutine");
        }

        sol::coroutine catch_co = co_result;

        auto catch_task = std::make_shared<AsyncTask>(scheduler, catch_co, context);
        catch_task->Start();
        return catch_task;
    };

    // --- Create 'async' namespace ---
    sol::table async_api = tas.create_named("async");

    // tas.async(fn) - Create async task
    async_api["create"] = [scheduler, context, logPrefix](sol::function fn) -> std::shared_ptr<AsyncTask> {
        sol::state &lua = context->GetLuaState();

        // Create coroutine from the function
        sol::coroutine co(lua, fn);
        auto task = std::make_shared<AsyncTask>(scheduler, co, context);

        Log::Info("%s Created async task", logPrefix.c_str());
        return task;
    };

    // tas.spawn(fn) - Create and start task immediately
    async_api["spawn"] = [scheduler, context, logPrefix](sol::function fn) -> std::shared_ptr<AsyncTask> {
        sol::state &lua = context->GetLuaState();

        // Create coroutine from the function
        sol::coroutine co(lua, fn);
        auto task = std::make_shared<AsyncTask>(scheduler, co, context);
        task->Start();

        Log::Info("%s Spawned async task", logPrefix.c_str());
        return task;
    };

    // --- Awaitable Primitives ---

    // tas.delay(ticks) - Delay for N ticks
    async_api["delay"] = [scheduler](int ticks) {
        scheduler->YieldTicks(ticks);
    };

    // tas.wait_for_event(event_name) - Wait for specific event
    async_api["wait_for_event"] = [context, scheduler](const std::string &event_name) {
        EventManager *eventMgr = context->GetEventManager();
        if (!eventMgr) {
            throw sol::error("EventManager not available");
        }

        bool event_fired = false;

        // Register one-time event handler (automatically removed after firing)
        auto handler = [&event_fired]() {
            event_fired = true;
        };

        eventMgr->RegisterOnceListener(event_name, std::function<void()>(handler));

        // Wait for event
        while (!event_fired) {
            scheduler->YieldTicks(1);
        }
    };

    // tas.wait_until(condition_fn) - Wait until condition is true
    async_api["wait_until"] = [scheduler](sol::function condition_fn) {
        while (true) {
            sol::protected_function_result result = condition_fn();
            if (result.valid() && result.get_type() == sol::type::boolean) {
                if (result.get<bool>()) {
                    break; // Condition met
                }
            }
            scheduler->YieldTicks(1);
        }
    };

    // --- Combinators ---

    // tas.async.all(tasks) - Wait for all tasks
    async_api["all"] = [scheduler](sol::table tasks) -> sol::table {
        try {
            sol::state_view lua = tasks.lua_state();
            sol::table results = lua.create_table();

            // Start all tasks
            for (const auto &pair : tasks) {
                sol::object obj = pair.second;
                if (obj.is<std::shared_ptr<AsyncTask>>()) {
                    auto task = obj.as<std::shared_ptr<AsyncTask>>();
                    if (task && task->IsPending()) {
                        task->Start();
                    }
                }
            }

            // Wait for all to complete
            bool all_done = false;
            while (!all_done) {
                all_done = true;
                for (const auto &pair : tasks) {
                    sol::object obj = pair.second;
                    if (obj.is<std::shared_ptr<AsyncTask>>()) {
                        auto task = obj.as<std::shared_ptr<AsyncTask>>();
                        if (task && !task->IsDone()) {
                            all_done = false;
                            break;
                        }
                    }
                }
                if (!all_done) {
                    scheduler->YieldTicks(1);
                }
            }

            // Collect results
            size_t index = 1;
            for (const auto &pair : tasks) {
                sol::object obj = pair.second;
                if (obj.is<std::shared_ptr<AsyncTask>>()) {
                    auto task = obj.as<std::shared_ptr<AsyncTask>>();
                    if (!task) {
                        throw sol::error("Task is null in all()");
                    }
                    if (task->IsCompleted()) {
                        results[index++] = task->GetResult();
                    } else if (task->IsFailed()) {
                        throw sol::error("Task failed in all(): " + task->GetError());
                    } else {
                        throw sol::error("Task was cancelled in all()");
                    }
                }
            }

            return results;
        } catch (const std::exception &e) {
            throw sol::error(std::string("async.all() failed: ") + e.what());
        }
    };

    // tas.async.race(tasks) - Wait for first task
    async_api["race"] = [scheduler](sol::table tasks) -> sol::object {
        try {
            // Start all tasks
            for (const auto &pair : tasks) {
                sol::object obj = pair.second;
                if (obj.is<std::shared_ptr<AsyncTask>>()) {
                    auto task = obj.as<std::shared_ptr<AsyncTask>>();
                    if (task && task->IsPending()) {
                        task->Start();
                    }
                }
            }

            // Wait for first to complete
            while (true) {
                for (const auto &pair : tasks) {
                    sol::object obj = pair.second;
                    if (obj.is<std::shared_ptr<AsyncTask>>()) {
                        auto task = obj.as<std::shared_ptr<AsyncTask>>();
                        if (!task) {
                            throw sol::error("Task is null in race()");
                        }
                        if (task->IsDone()) {
                            // Cancel other tasks
                            for (const auto &p2 : tasks) {
                                sol::object obj2 = p2.second;
                                if (obj2.is<std::shared_ptr<AsyncTask>>()) {
                                    auto task2 = obj2.as<std::shared_ptr<AsyncTask>>();
                                    if (task2 && task2.get() != task.get()) {
                                        task2->Cancel();
                                    }
                                }
                            }

                            // Return first result
                            if (task->IsCompleted()) {
                                return task->GetResult();
                            } else if (task->IsFailed()) {
                                throw sol::error("First task failed: " + task->GetError());
                            } else {
                                throw sol::error("First task was cancelled");
                            }
                        }
                    }
                }
                scheduler->YieldTicks(1);
            }
        } catch (const std::exception &e) {
            throw sol::error(std::string("async.race() failed: ") + e.what());
        }
    };

    // tas.async.any(tasks) - Wait for first successful task
    async_api["any"] = [scheduler](sol::table tasks) -> sol::object {
        try {
            // Start all tasks
            for (const auto &pair : tasks) {
                sol::object obj = pair.second;
                if (obj.is<std::shared_ptr<AsyncTask>>()) {
                    auto task = obj.as<std::shared_ptr<AsyncTask>>();
                    if (task && task->IsPending()) {
                        task->Start();
                    }
                }
            }

            std::vector<std::string> errors;

            // Wait for first successful completion
            while (true) {
                bool all_failed = true;

                for (const auto &pair : tasks) {
                    sol::object obj = pair.second;
                    if (obj.is<std::shared_ptr<AsyncTask>>()) {
                        auto task = obj.as<std::shared_ptr<AsyncTask>>();
                        if (!task) {
                            throw sol::error("Task is null in any()");
                        }

                        if (task->IsCompleted()) {
                            // Found successful task
                            // Cancel others
                            for (const auto &p2 : tasks) {
                                sol::object obj2 = p2.second;
                                if (obj2.is<std::shared_ptr<AsyncTask>>()) {
                                    auto task2 = obj2.as<std::shared_ptr<AsyncTask>>();
                                    if (task2 && task2.get() != task.get()) {
                                        task2->Cancel();
                                    }
                                }
                            }
                            return task->GetResult();
                        }

                        if (!task->IsDone()) {
                            all_failed = false;
                        } else if (task->IsFailed()) {
                            errors.push_back(task->GetError());
                        }
                    }
                }

                if (all_failed) {
                    // All tasks failed
                    std::string combined = "async.any: all tasks failed:\n";
                    for (size_t i = 0; i < errors.size(); ++i) {
                        combined += "  [" + std::to_string(i + 1) + "] " + errors[i] + "\n";
                    }
                    throw sol::error(combined);
                }

                scheduler->YieldTicks(1);
            }
        } catch (const std::exception &e) {
            throw sol::error(std::string("async.any() failed: ") + e.what());
        }
    };

    // Shorthand: tas.async(fn) as alias for tas.async.create(fn)
    tas["async"] = async_api["create"];

    // Shorthand: tas.await(task/delay)
    tas["await"] = [scheduler](sol::object obj) -> sol::object {
        if (obj.is<std::shared_ptr<AsyncTask>>()) {
            // Await task
            auto task = obj.as<std::shared_ptr<AsyncTask>>();
            if (task->IsPending()) {
                task->Start();
            }
            while (!task->IsDone()) {
                scheduler->YieldTicks(1);
            }
            if (task->IsCompleted()) {
                return task->GetResult();
            } else if (task->IsFailed()) {
                throw sol::error("Task failed: " + task->GetError());
            } else {
                throw sol::error("Task was cancelled");
            }
        } else if (obj.is<int>()) {
            // Await delay (ticks)
            int ticks = obj.as<int>();
            scheduler->YieldTicks(ticks);
            return sol::nil;
        } else {
            throw sol::error("await requires AsyncTask or int (ticks)");
        }
    };
}
