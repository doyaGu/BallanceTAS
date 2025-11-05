#include "LuaApi.h"

#include "Logger.h"
#include <stdexcept>

#include "TASEngine.h"
#include "GameInterface.h"
#include "LuaScheduler.h"
#include "ScriptContext.h"

// ===================================================================
// Level Control API Registration (Stub Implementation)
// ===================================================================

void LuaApi::RegisterLevelApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterLevelApi requires a valid ScriptContext");
    }

    // Create nested 'level' table
    sol::table level = tas["level"] = tas.create();

    // ===================================================================
    // Level Query Functions (Implemented)
    // ===================================================================

    // tas.level.get_current() - Get current level name
    level["get_current"] = [context]() -> std::string {
        auto *g = context->GetGameInterface();
        if (!g) {
            return "";
        }
        return g->GetMapName();
    };

    // tas.level.get_current_number() - Get current level number (1-13)
    level["get_current_number"] = [context]() -> int {
        auto *g = context->GetGameInterface();
        if (!g) {
            return 0;
        }
        return g->GetCurrentLevel();
    };

    // tas.level.is_loaded() - Check if a level is currently loaded
    level["is_loaded"] = [context]() -> bool {
        auto *g = context->GetGameInterface();
        if (!g) {
            return false;
        }
        return g->IsPlaying();
    };

    // tas.level.is_paused() - Check if the level is paused
    level["is_paused"] = [context]() -> bool {
        auto *g = context->GetGameInterface();
        if (!g) {
            return false;
        }
        return g->IsPaused();
    };

    // tas.level.get_sector() - Get current sector/checkpoint
    level["get_sector"] = [context]() -> int {
        auto *g = context->GetGameInterface();
        if (!g) {
            return 0;
        }
        return g->GetCurrentSector();
    };

    // ===================================================================
    // Level Control Functions (Stub Implementation)
    // ===================================================================

    // tas.level.load(level_name) - Load a specific level
    level["load"] = [](const std::string &levelName) {
        Log::Warn("[STUB] level.load('%s') - Not yet implemented", levelName.c_str());
        Log::Info("  Future implementation: This will trigger level loading through the game engine");
        throw sol::error("level.load: Not yet implemented - stub function");
    };

    // tas.level.restart() - Restart the current level
    level["restart"] = []() {
        Log::Warn("[STUB] level.restart() - Not yet implemented");
        Log::Info("  Future implementation: This will restart the current level");
        throw sol::error("level.restart: Not yet implemented - stub function");
    };

    // tas.level.exit_to_menu() - Exit to main menu
    level["exit_to_menu"] = []() {
        Log::Warn("[STUB] level.exit_to_menu() - Not yet implemented");
        Log::Info("  Future implementation: This will exit to the main menu");
        throw sol::error("level.exit_to_menu: Not yet implemented - stub function");
    };

    // tas.level.next() - Load the next level
    level["next"] = []() {
        Log::Warn("[STUB] level.next() - Not yet implemented");
        Log::Info("  Future implementation: This will load the next level in sequence");
        throw sol::error("level.next: Not yet implemented - stub function");
    };

    // tas.level.previous() - Load the previous level
    level["previous"] = []() {
        Log::Warn("[STUB] level.previous() - Not yet implemented");
        Log::Info("  Future implementation: This will load the previous level");
        throw sol::error("level.previous: Not yet implemented - stub function");
    };

    // ===================================================================
    // Level State Query Functions (Stub Implementation)
    // ===================================================================

    // tas.level.is_completed() - Check if level is completed
    level["is_completed"] = []() -> bool {
        Log::Warn("[STUB] level.is_completed() - Not yet implemented, returning false");
        Log::Info("  Future implementation: This will check if the level end was reached");
        return false; // stub
    };

    // tas.level.is_at_checkpoint(sector) - Check if at a specific checkpoint
    level["is_at_checkpoint"] = [context](int sector) -> bool {
        auto *g = context->GetGameInterface();
        if (!g) {
            return false;
        }
        int currentSector = g->GetCurrentSector();
        return currentSector == sector;
    };

    // ===================================================================
    // Level Wait Functions (Using Event System)
    // ===================================================================

    // tas.level.wait_for_load() - Wait until a level is loaded
    level["wait_for_load"] = sol::yielding([context]() {
        auto *scheduler = context->GetScheduler();
        if (!scheduler) {
            throw sol::error("level.wait_for_load: Scheduler not available");
        }

        // Wait until level is loaded
        sol::function predicate = sol::make_object(context->GetLuaState(), [context]() {
            auto *g = context->GetGameInterface();
            return g && g->IsPlaying();
        });
        scheduler->YieldUntil(predicate);
    });

    // tas.level.wait_for_complete() - Wait until level is completed
    level["wait_for_complete"] = sol::yielding([context]() {
        auto *scheduler = context->GetScheduler();
        if (!scheduler) {
            throw sol::error("level.wait_for_complete: Scheduler not available");
        }
        // Use event system to wait for level completion
        scheduler->YieldWaitForEvent("level_complete");
    });

    // tas.level.wait_for_checkpoint(sector) - Wait until reaching a checkpoint
    level["wait_for_checkpoint"] = sol::yielding([context](int targetSector) {
        auto *scheduler = context->GetScheduler();
        if (!scheduler) {
            throw sol::error("level.wait_for_checkpoint: Scheduler not available");
        }

        // Wait until reaching the target sector
        sol::function predicate = sol::make_object(context->GetLuaState(), [context, targetSector]() {
            auto *g = context->GetGameInterface();
            if (!g) {
                return false;
            }
            return g->GetCurrentSector() >= targetSector;
        });
        scheduler->YieldUntil(predicate);
    });
}
