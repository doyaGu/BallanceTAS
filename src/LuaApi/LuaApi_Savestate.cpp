#include "LuaApi.h"

#include "Logger.h"
#include "TASEngine.h"
#include "LuaScheduler.h"
#include "SavestateManager.h"
#include "ScriptContext.h"
#include "ServiceContainer.h"

/**
 * @brief Register savestate API to Lua
 *
 * Provides tas.savestate.* functions for quick state save/load.
 *
 * API:
 * - tas.savestate.save(name, description?) - Save current state
 * - tas.savestate.load(name) - Load state (yielding, waits for stabilization)
 * - tas.savestate.delete(name) - Delete state
 * - tas.savestate.list() - List all states
 * - tas.savestate.exists(name) - Check if state exists
 * - tas.savestate.get_info(name) - Get state metadata
 */
void LuaApi::RegisterSavestateApi(sol::table &tas, ScriptContext *context) {
    auto serviceProvider = context->GetEngine()->GetServiceProvider();
    auto savestateManager = serviceProvider->Resolve<SavestateManager>();

    if (!savestateManager) {
        Log::Error("SavestateManager not available for Lua API");
        return;
    }

    // Create tas.savestate table
    auto savestate_table = tas["savestate"].get_or_create<sol::table>();

    // ========================================================================
    // tas.savestate.save(name, description?)
    // ========================================================================
    savestate_table["save"] = [savestateManager, context](
        const string& name,
        sol::optional<string> description
    ) -> sol::object {
        if (!context) {
            return sol::make_object(context->GetLuaState(), "Context not available");
        }

        auto result = description
            ? savestateManager->SaveState(name, *description)
            : savestateManager->SaveState(name);

        if (result.IsOk()) {
            return sol::make_object(context->GetLuaState(), sol::nil);
        } else {
            return sol::make_object(context->GetLuaState(), result.GetError().message);
        }
    };

    // ========================================================================
    // tas.savestate.load(name) - Yielding function
    // ========================================================================
    savestate_table["load"] = [savestateManager, context](const string& name) {
        if (!context) {
            throw std::runtime_error("Context not available");
        }

        // Load state
        auto result = savestateManager->LoadState(name);

        if (result.IsError()) {
            throw std::runtime_error("Failed to load savestate: " + result.GetError().message);
        }

        // Wait a few frames for physics to stabilize
        // This is a yielding operation
        auto* scheduler = context->GetScheduler();
        if (scheduler) {
            scheduler->YieldTicks(5);  // Wait 5 ticks
        }

        Log::Info("[%s] Loaded savestate: %s", context->GetName().c_str(), name.c_str());
    };

    // ========================================================================
    // tas.savestate.delete(name)
    // ========================================================================
    savestate_table["del"] = [savestateManager, context](const string& name) -> sol::object {
        if (!context) {
            return sol::make_object(context->GetLuaState(),
                                   "Context not available");
        }

        auto result = savestateManager->DeleteState(name);

        if (result.IsOk()) {
            return sol::make_object(context->GetLuaState(), sol::nil);
        } else {
            return sol::make_object(context->GetLuaState(), result.GetError().message);
        }
    };

    // Alias: delete is a keyword, so also provide "remove"
    savestate_table["remove"] = savestate_table["del"];

    // ========================================================================
    // tas.savestate.list()
    // ========================================================================
    savestate_table["list"] = [savestateManager, context]() -> sol::object {
        if (!context) {
            return sol::make_object(context->GetLuaState(), sol::nil);
        }

        auto result = savestateManager->ListStates();

        if (result.IsError()) {
            Log::Error("Failed to list savestates: %s", result.GetError().message.c_str());
            return sol::make_object(context->GetLuaState(), sol::nil);
        }

        auto states = result.Unwrap();

        // Convert to Lua table
        sol::state_view lua = context->GetLuaState();
        sol::table table = lua.create_table();

        for (size_t i = 0; i < states.size(); ++i) {
            table[i + 1] = states[i];  // Lua is 1-indexed
        }

        return table;
    };

    // ========================================================================
    // tas.savestate.exists(name)
    // ========================================================================
    savestate_table["exists"] = [savestateManager, context](const string& name) -> bool {
        if (!context) {
            return false;
        }

        return savestateManager->StateExists(name);
    };

    // ========================================================================
    // tas.savestate.get_info(name)
    // ========================================================================
    savestate_table["get_info"] = [savestateManager, context](
        const string& name
    ) -> sol::object {
        if (!context) {
            return sol::make_object(context->GetLuaState(), sol::nil);
        }

        auto result = savestateManager->GetStateInfo(name);

        if (result.IsError()) {
            Log::Error("Failed to get savestate info: %s", result.GetError().message.c_str());
            return sol::make_object(context->GetLuaState(), sol::nil);
        }

        auto data = result.Unwrap();
        sol::state_view lua = context->GetLuaState();

        // Create info table
        sol::table info = lua.create_table();

        info["name"] = data.name;
        info["timestamp"] = data.timestamp;
        info["level_name"] = data.levelName;
        info["level_number"] = data.levelNumber;
        info["description"] = data.description;

        // Position
        sol::table pos_table = lua.create_table();
        pos_table["x"] = data.position.x;
        pos_table["y"] = data.position.y;
        pos_table["z"] = data.position.z;
        info["position"] = pos_table;

        // Game state
        info["points"] = data.points;
        info["lives"] = data.lives;
        info["sector"] = data.sector;
        info["sr_score"] = data.srScore;
        info["hs_score"] = data.hsScore;

        info["tick"] = data.tick;

        return info;
    };

    // ========================================================================
    // tas.savestate.get_directory()
    // ========================================================================
    savestate_table["get_directory"] = [savestateManager]() -> string {
        return savestateManager->GetSavestatesDirectory();
    };

    Log::Info("Savestate API registered");
}
