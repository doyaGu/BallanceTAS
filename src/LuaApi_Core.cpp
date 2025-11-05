#include "LuaApi.h"

#include <sstream>
#include <stdexcept>
#include <chrono>

#include <fmt/format.h>
#include <fmt/args.h>

#include "Logger.h"
#include "TASEngine.h"
#include "ProjectManager.h"
#include "TASProject.h"
#include "ScriptContext.h"
#include "GameInterface.h"

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
                    try {
                        // CRITICAL FIX: Convert to protected_function before calling
                        sol::protected_function tostringFunc = tostring;
                        sol::protected_function_result res = tostringFunc(ud);
                        if (res.valid() && res.get_type() == sol::type::string) {
                            store.push_back(res.get<std::string>());
                            break;
                        }
                    } catch (const std::exception &) {
                        // Fall through to default userdata representation
                    }
                }

                store.push_back("<userdata>");
                break;
            }
            case sol::type::function: {
                store.push_back("<function>");
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
//  Core & Flow Control API Registration
// ===================================================================

void LuaApi::RegisterCoreApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterCoreApi requires a valid ScriptContext");
    }

    std::string logPrefix = "[" + context->GetName() + "]";

    // tas.log(format_string, ...)
    tas["log"] = [logPrefix](const std::string &fmt, sol::variadic_args va) {
        try {
            std::string text = FormatString(fmt, va);
            Log::Info("%s %s", logPrefix.c_str(), text.c_str());
        } catch (const std::exception &e) {
            Log::Error("%s Error in log: %s", logPrefix.c_str(), e.what());
        }
    };

    // tas.warn(format_string, ...)
    tas["warn"] = [logPrefix](const std::string &fmt, sol::variadic_args va) {
        try {
            std::string text = FormatString(fmt, va);
            Log::Warn("%s %s", logPrefix.c_str(), text.c_str());
        } catch (const std::exception &e) {
            Log::Error("%s Error in warn: %s", logPrefix.c_str(), e.what());
        }
    };

    // tas.error(format_string, ...)
    tas["error"] = [logPrefix](const std::string &fmt, sol::variadic_args va) {
        try {
            std::string text = FormatString(fmt, va);
            Log::Error("%s %s", logPrefix.c_str(), text.c_str());
        } catch (const std::exception &e) {
            Log::Error("%s Error in error: %s", logPrefix.c_str(), e.what());
        }
    };

    // tas.print(format_string, ...)
    tas["print"] = [logPrefix, context](const std::string &fmt, sol::variadic_args va) {
        try {
            std::string text = FormatString(fmt, va);
            context->GetGameInterface()->PrintMessage(text.c_str());
        } catch (const std::exception &e) {
            Log::Error("%s Error in print: %s", logPrefix.c_str(), e.what());
        }
    };

    // tas.get_tick()
    tas["get_tick"] = [context]() -> unsigned int {
        return context->GetCurrentTick();
    };

    // tas.get_manifest()
    tas["get_manifest"] = [context]() -> sol::table {
        try {
            const TASProject *project = context->GetCurrentProject();
            if (project) {
                return project->GetManifestTable();
            }
        } catch (const std::exception &) {
            // Fall through to return empty table
        }
        return context->GetLuaState().create_table();
    };
}

// ===================================================================
//  Debug & Assertions API Registration
// ===================================================================

void LuaApi::RegisterDebugApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterDebugApi requires a valid ScriptContext");
    }

    // tas.assert(condition, message)
    tas["assert"] = [](bool condition, sol::optional<std::string> message) {
        if (condition) return;

        std::string error_message = message.value_or("Assertion failed!");
        throw sol::error(error_message);
    };

    // tas.skip_rendering(ticks)
    tas["skip_rendering"] = [context](size_t ticks) {
        auto *g = context->GetGameInterface();
        if (g) {
            g->SkipRenderForTicks(ticks);
        }
    };

    // === Phase 3 Sprint 3: Enhanced Debug APIs ===

    // tas.get_stack_trace() - Get current stack trace
    tas["get_stack_trace"] = [context](sol::optional<int> max_depth) {
        sol::state_view lua = context->GetLuaState();
        sol::table trace = lua.create_table();

        int depth = max_depth.value_or(20);  // Default max 20 frames
        int level = 1;  // Start at calling function (skip this C function)

        while (level <= depth) {
            lua_Debug ar;
            if (lua_getstack(lua.lua_state(), level, &ar) == 0) {
                break;  // No more stack frames
            }

            lua_getinfo(lua.lua_state(), "nSl", &ar);

            sol::table frame = lua.create_table();
            frame["level"] = level;
            frame["name"] = ar.name ? ar.name : "<unknown>";
            frame["source"] = ar.source ? ar.source : "<unknown>";
            frame["short_src"] = ar.short_src;
            frame["line"] = ar.currentline;
            frame["what"] = ar.what ? ar.what : "?";

            trace.add(frame);
            level++;
        }

        return trace;
    };

    // tas.memory_snapshot() - Take memory snapshot
    tas["memory_snapshot"] = [context]() -> sol::table {
        sol::state_view lua = context->GetLuaState();
        sol::table snapshot = lua.create_table();

        // Get total memory usage in KB
        int mem_kb = lua_gc(lua.lua_state(), LUA_GCCOUNT, 0);
        int mem_bytes = lua_gc(lua.lua_state(), LUA_GCCOUNTB, 0);
        double total_kb = mem_kb + (mem_bytes / 1024.0);

        snapshot["total_kb"] = total_kb;
        snapshot["total_bytes"] = mem_kb * 1024 + mem_bytes;
        snapshot["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        return snapshot;
    };

    // tas.memory_diff(snapshot1, snapshot2) - Compare memory snapshots
    tas["memory_diff"] = [context](sol::table snapshot1, sol::table snapshot2) -> sol::table {
        sol::state_view lua = context->GetLuaState();
        sol::table diff = lua.create_table();

        double kb1 = snapshot1["total_kb"].get_or(0.0);
        double kb2 = snapshot2["total_kb"].get_or(0.0);

        diff["delta_kb"] = kb2 - kb1;
        diff["delta_bytes"] = (kb2 - kb1) * 1024;
        diff["percentage"] = kb1 > 0 ? ((kb2 - kb1) / kb1) * 100.0 : 0.0;

        return diff;
    };

    // tas.profile(function) - Profile function execution
    tas["profile"] = [context](sol::function func) -> sol::table {
        sol::state_view lua = context->GetLuaState();

        // Take initial snapshot
        auto start_time = std::chrono::high_resolution_clock::now();
        int mem_before_kb = lua_gc(lua.lua_state(), LUA_GCCOUNT, 0);
        int mem_before_bytes = lua_gc(lua.lua_state(), LUA_GCCOUNTB, 0);
        double mem_before = mem_before_kb + (mem_before_bytes / 1024.0);

        // Execute function
        sol::protected_function_result result = func();

        // Take final snapshot
        auto end_time = std::chrono::high_resolution_clock::now();
        int mem_after_kb = lua_gc(lua.lua_state(), LUA_GCCOUNT, 0);
        int mem_after_bytes = lua_gc(lua.lua_state(), LUA_GCCOUNTB, 0);
        double mem_after = mem_after_kb + (mem_after_bytes / 1024.0);

        // Calculate duration
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();

        // Build result table
        sol::table stats = lua.create_table();
        stats["duration_us"] = duration;
        stats["duration_ms"] = duration / 1000.0;
        stats["memory_before_kb"] = mem_before;
        stats["memory_after_kb"] = mem_after;
        stats["memory_delta_kb"] = mem_after - mem_before;
        stats["success"] = result.valid();

        if (!result.valid()) {
            sol::error err = result;
            stats["error"] = err.what();
        }

        return stats;
    };

    // tas.force_gc() - Force garbage collection
    tas["force_gc"] = [context]() -> int {
        lua_State* L = context->GetLuaState().lua_state();
        return lua_gc(L, LUA_GCCOLLECT, 0);
    };

    // tas.get_memory_usage() - Get current memory usage
    tas["get_memory_usage"] = [context]() -> double {
        lua_State* L = context->GetLuaState().lua_state();
        int kb = lua_gc(L, LUA_GCCOUNT, 0);
        int bytes = lua_gc(L, LUA_GCCOUNTB, 0);
        return kb + (bytes / 1024.0);
    };
}

// ===================================================================
//  Global State Management API Registration
// ===================================================================

void LuaApi::RegisterGlobalApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterGlobalApi requires a valid ScriptContext");
    }

    // Create nested 'global' table
    sol::table global = tas["global"] = tas.create();

    // CRITICAL: Capture context to ensure proper Lua state isolation.
    // Each context has its own global data registry for complete isolation.

    // tas.global.set_data(key, value) - Store persistent data across levels
    global["set_data"] = [context](const std::string &key, sol::object value) {
        if (key.empty()) {
            throw sol::error("global.set_data: key cannot be empty");
        }

        // Use context's Lua state
        auto &luaState = context->GetLuaState();
        // Store in a special global registry table
        sol::table registry = luaState["__tas_global_registry"];
        if (!registry.valid()) {
            registry = luaState.create_table();
            luaState["__tas_global_registry"] = registry;
        }
        registry[key] = value;
    };

    // tas.global.get_data(key) - Retrieve persistent data
    global["get_data"] = [context](const std::string &key) -> sol::object {
        if (key.empty()) {
            throw sol::error("global.get_data: key cannot be empty");
        }

        // Use context's Lua state
        auto &luaState = context->GetLuaState();
        sol::table registry = luaState["__tas_global_registry"];
        if (!registry.valid()) {
            return sol::nil;
        }

        sol::object value = registry[key];
        return value.valid() ? value : sol::nil;
    };

    // tas.global.has_data(key) - Check if data exists
    global["has_data"] = [context](const std::string &key) -> bool {
        if (key.empty()) {
            return false;
        }

        // Use context's Lua state
        auto &luaState = context->GetLuaState();
        sol::table registry = luaState["__tas_global_registry"];
        if (!registry.valid()) {
            return false;
        }

        sol::object value = registry[key];
        return value.valid() && !value.is<sol::nil_t>();
    };

    // tas.global.clear_data(key) - Clear specific data
    global["clear_data"] = [context](const std::string &key) {
        if (key.empty()) {
            throw sol::error("global.clear_data: key cannot be empty");
        }

        // Use context's Lua state
        auto &luaState = context->GetLuaState();
        sol::table registry = luaState["__tas_global_registry"];
        if (registry.valid()) {
            registry[key] = sol::nil;
        }
    };

    // tas.global.clear_all_data() - Clear all global data
    global["clear_all_data"] = [context]() {
        // Use context's Lua state
        auto &luaState = context->GetLuaState();
        luaState["__tas_global_registry"] = sol::nil;
    };

    // tas.global.get_all_keys() - Get all stored data keys
    global["get_all_keys"] = [context]() -> std::vector<std::string> {
        std::vector<std::string> keys;

        // Use context's Lua state
        auto &luaState = context->GetLuaState();
        sol::table registry = luaState["__tas_global_registry"];
        if (!registry.valid()) {
            return keys;
        }

        for (const auto &pair : registry) {
            if (pair.first.is<std::string>()) {
                keys.push_back(pair.first.as<std::string>());
            }
        }

        return keys;
    };
}

// ===================================================================
//  GC (Garbage Collection) API Registration
// ===================================================================

void LuaApi::RegisterGCApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterGCApi requires a valid ScriptContext");
    }

    std::string logPrefix = "[" + context->GetName() + "]";

    // Create nested 'gc' table
    sol::table gc = tas["gc"] = tas.create();

    // --- GC Control APIs ---

    // tas.gc.collect() - Perform a full garbage collection cycle
    gc["collect"] = [context, logPrefix]() {
        try {
            lua_State *L = context->GetLuaState().lua_state();
            lua_gc(L, LUA_GCCOLLECT, 0);
            Log::Info("%s Full GC cycle completed.", logPrefix.c_str());
        } catch (const std::exception &e) {
            Log::Error("%s Error in gc.collect: %s", logPrefix.c_str(), e.what());
            throw;
        }
    };

    // tas.gc.stop() - Stop automatic garbage collection
    gc["stop"] = [context, logPrefix]() {
        try {
            lua_State *L = context->GetLuaState().lua_state();
            lua_gc(L, LUA_GCSTOP, 0);
            Log::Info("%s GC stopped.", logPrefix.c_str());
        } catch (const std::exception &e) {
            Log::Error("%s Error in gc.stop: %s", logPrefix.c_str(), e.what());
            throw;
        }
    };

    // tas.gc.restart() - Restart automatic garbage collection
    gc["restart"] = [context, logPrefix]() {
        try {
            lua_State *L = context->GetLuaState().lua_state();
            lua_gc(L, LUA_GCRESTART, 0);
            Log::Info("%s GC restarted.", logPrefix.c_str());
        } catch (const std::exception &e) {
            Log::Error("%s Error in gc.restart: %s", logPrefix.c_str(), e.what());
            throw;
        }
    };

    // tas.gc.step(step_size) - Perform an incremental GC step
    // step_size: Amount of work to perform (in KB for Lua 5.4+, arbitrary units for 5.3)
    gc["step"] = [context, logPrefix](sol::optional<int> stepSize) {
        try {
            lua_State *L = context->GetLuaState().lua_state();
            int size = stepSize.value_or(1);  // Default 1KB step
            if (size < 0) {
                throw sol::error("gc.step: step_size must be non-negative");
            }

            // LUA_GCSTEP performs a single incremental step
            // Return value: 1 if GC cycle completed, 0 otherwise
            int completed = lua_gc(L, LUA_GCSTEP, size);

            if (completed) {
                Log::Info("%s GC step completed a full cycle.", logPrefix.c_str());
            }

            return completed != 0;
        } catch (const std::exception &e) {
            Log::Error("%s Error in gc.step: %s", logPrefix.c_str(), e.what());
            throw;
        }
    };

    // --- GC Mode APIs (Lua 5.4+) ---

    // tas.gc.set_mode(mode) - Set GC mode ("generational" or "incremental")
    gc["set_mode"] = [context, logPrefix](const std::string &mode) {
        try {
            #if LUA_VERSION_NUM >= 504
                lua_State *L = context->GetLuaState().lua_state();

                if (mode == "generational") {
                    lua_gc(L, LUA_GCGEN, 0, 0);
                    Log::Info("%s GC mode set to Generational.", logPrefix.c_str());
                    return true;
                } else if (mode == "incremental") {
                    lua_gc(L, LUA_GCINC, 0, 0, 0);
                    Log::Info("%s GC mode set to Incremental.", logPrefix.c_str());
                    return true;
                } else {
                    throw sol::error("gc.set_mode: mode must be 'generational' or 'incremental'");
                }
            #else
                Log::Warn("%s gc.set_mode: Lua version < 5.4, only incremental mode available.", logPrefix.c_str());
                return false;
            #endif
        } catch (const std::exception &e) {
            Log::Error("%s Error in gc.set_mode: %s", logPrefix.c_str(), e.what());
            throw;
        }
    };

    // tas.gc.get_mode() - Get current GC mode
    gc["get_mode"] = [context]() -> std::string {
        LuaGCMode mode = context->GetGCMode();
        return mode == LuaGCMode::Generational ? "generational" : "incremental";
    };

    // --- GC Tuning APIs ---

    // tas.gc.tune(params) - Tune GC parameters
    // params: table with optional keys: pause, stepmul, minormul, majormul
    gc["tune"] = [context, logPrefix](sol::table params) {
        try {
            lua_State *L = context->GetLuaState().lua_state();
            sol::table result = context->GetLuaState().create_table();

            #if LUA_VERSION_NUM >= 504
                // Lua 5.4+ parameters

                // pause: How long to wait before starting a new GC cycle (percentage)
                // Default: 200 (waits until memory doubles)
                if (sol::optional<int> pause = params["pause"]) {
                    int value = pause.value();
                    if (value < 0) {
                        throw sol::error("gc.tune: pause must be non-negative");
                    }
                    int oldPause = lua_gc(L, LUA_GCSETPAUSE, value);
                    result["old_pause"] = oldPause;
                    Log::Info("%s GC pause set to %d%% (was %d%%)",
                                             logPrefix.c_str(), value, oldPause);
                }

                // stepmul: GC speed multiplier (percentage)
                // Default: 200 (GC runs 2x faster than memory allocation)
                if (sol::optional<int> stepmul = params["stepmul"]) {
                    int value = stepmul.value();
                    if (value < 0) {
                        throw sol::error("gc.tune: stepmul must be non-negative");
                    }
                    int oldStepmul = lua_gc(L, LUA_GCSETSTEPMUL, value);
                    result["old_stepmul"] = oldStepmul;
                    Log::Info("%s GC stepmul set to %d%% (was %d%%)",
                                             logPrefix.c_str(), value, oldStepmul);
                }

                // Generational GC parameters (Lua 5.4+)
                if (sol::optional<int> minormul = params["minormul"]) {
                    int value = minormul.value();
                    if (value < 0) {
                        throw sol::error("gc.tune: minormul must be non-negative");
                    }
                    // minormul: Minor collection multiplier
                    lua_gc(L, LUA_GCGEN, value, 0);
                    Log::Info("%s GC minormul set to %d%%", logPrefix.c_str(), value);
                }

                if (sol::optional<int> majormul = params["majormul"]) {
                    int value = majormul.value();
                    if (value < 0) {
                        throw sol::error("gc.tune: majormul must be non-negative");
                    }
                    // majormul: Major collection multiplier
                    lua_gc(L, LUA_GCGEN, 0, value);
                    Log::Info("%s GC majormul set to %d%%", logPrefix.c_str(), value);
                }
            #else
                // Lua 5.3 parameters
                if (sol::optional<int> pause = params["pause"]) {
                    int value = pause.value();
                    if (value < 0) {
                        throw sol::error("gc.tune: pause must be non-negative");
                    }
                    int oldPause = lua_gc(L, LUA_GCSETPAUSE, value);
                    result["old_pause"] = oldPause;
                }

                if (sol::optional<int> stepmul = params["stepmul"]) {
                    int value = stepmul.value();
                    if (value < 0) {
                        throw sol::error("gc.tune: stepmul must be non-negative");
                    }
                    int oldStepmul = lua_gc(L, LUA_GCSETSTEPMUL, value);
                    result["old_stepmul"] = oldStepmul;
                }

                // minormul/majormul not available in Lua 5.3
                if (params["minormul"].valid() || params["majormul"].valid()) {
                    Log::Warn("%s gc.tune: minormul/majormul require Lua 5.4+",
                                             logPrefix.c_str());
                }
            #endif

            return result;
        } catch (const std::exception &e) {
            Log::Error("%s Error in gc.tune: %s", logPrefix.c_str(), e.what());
            throw;
        }
    };

    // --- GC Metrics/Stats APIs ---

    // tas.gc.stats() - Get detailed GC statistics
    gc["stats"] = [context](sol::this_state s) -> sol::table {
        sol::state_view lua(s);
        sol::table stats = lua.create_table();

        try {
            lua_State *L = context->GetLuaState().lua_state();

            // Memory usage in KB
            int memKB = lua_gc(L, LUA_GCCOUNT, 0);
            int memBytes = lua_gc(L, LUA_GCCOUNTB, 0);  // Remainder bytes

            stats["memory_kb"] = static_cast<double>(memKB) + (static_cast<double>(memBytes) / 1024.0);
            stats["memory_bytes"] = (memKB * 1024) + memBytes;

            // GC mode
            LuaGCMode mode = context->GetGCMode();
            stats["mode"] = mode == LuaGCMode::Generational ? "generational" : "incremental";

            // Check if GC is running
            #if LUA_VERSION_NUM >= 502
                stats["running"] = lua_gc(L, LUA_GCISRUNNING, 0) != 0;
            #else
                stats["running"] = true;  // Assume running for Lua 5.1
            #endif

            // Context info
            stats["context_name"] = context->GetName();
            stats["context_type"] = [type = context->GetType()]() {
                switch (type) {
                    case ScriptContextType::Global: return "global";
                    case ScriptContextType::Level: return "level";
                    case ScriptContextType::Custom: return "custom";
                    default: return "unknown";
                }
            }();

        } catch (const std::exception &e) {
            Log::Error("Error in gc.stats: %s", e.what());
        }

        return stats;
    };

    // tas.gc.get_memory_kb() - Get current memory usage in KB (simple accessor)
    gc["get_memory_kb"] = [context]() -> double {
        return context->GetLuaMemoryKB();
    };

    // tas.gc.get_memory_bytes() - Get current memory usage in bytes
    gc["get_memory_bytes"] = [context]() -> size_t {
        return context->GetLuaMemoryBytes();
    };

    // tas.gc.is_running() - Check if GC is currently running
    gc["is_running"] = [context]() -> bool {
        try {
            lua_State *L = context->GetLuaState().lua_state();
            #if LUA_VERSION_NUM >= 502
                return lua_gc(L, LUA_GCISRUNNING, 0) != 0;
            #else
                return true;  // Assume running for Lua 5.1
            #endif
        } catch (const std::exception &) {
            return false;
        }
    };
}
