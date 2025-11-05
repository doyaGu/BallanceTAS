#include "LuaApi.h"

#include <stdexcept>
#include <chrono>

#include <fmt/format.h>
#include <fmt/args.h>

#include "Logger.h"
#include "TASEngine.h"
#include "ScriptContext.h"

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
