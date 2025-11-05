#include "LuaApi.h"

#include <stdexcept>
#include <chrono>

#include <fmt/format.h>
#include <fmt/args.h>

#include "TASEngine.h"
#include "ScriptContext.h"
#include "GameInterface.h"

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
