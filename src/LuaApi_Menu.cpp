#include "LuaApi.h"

#include "Logger.h"
#include <stdexcept>

#include "TASEngine.h"
#include "GameInterface.h"
#include "LuaScheduler.h"
#include "ScriptContext.h"

// ===================================================================
// Menu Navigation API Registration (Stub Implementation)
// ===================================================================

void LuaApi::RegisterMenuApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterMenuApi requires a valid ScriptContext");
    }

    // Create nested 'menu' table
    sol::table menu = tas["menu"] = tas.create();

    // ===================================================================
    // Menu Query Functions (Implemented)
    // ===================================================================

    // tas.menu.is_in_menu() - Check if currently in menu
    menu["is_in_menu"] = [context]() -> bool {
        auto *g = context->GetGameInterface();
        if (!g) {
            return false;
        }
        // If not playing, we're likely in the menu
        return !g->IsPlaying();
    };

    // tas.menu.is_in_game() - Check if currently in game
    menu["is_in_game"] = [context]() -> bool {
        auto *g = context->GetGameInterface();
        if (!g) {
            return false;
        }
        return g->IsPlaying();
    };

    // ===================================================================
    // Menu State Query Functions (Stub Implementation)
    // ===================================================================

    // tas.menu.get_current() - Get current menu identifier
    menu["get_current"] = []() -> std::string {
        Log::Warn("[STUB] menu.get_current() - Not yet implemented, returning 'unknown'");
        Log::Info("  Future implementation: This will detect the current menu screen");
        return "unknown"; // stub
    };

    // tas.menu.is_at(menu_name) - Check if at a specific menu
    menu["is_at"] = [](const std::string &menuName) -> bool {
        Log::Warn("[STUB] menu.is_at('%s') - Not yet implemented, returning false", menuName.c_str());
        Log::Info("  Future implementation: This will check if at the specified menu");
        return false; // stub
    };

    // ===================================================================
    // Menu Navigation Functions (Stub Implementation)
    // ===================================================================

    // tas.menu.navigate_to(menu_path) - Navigate to a menu by path
    menu["navigate_to"] = [](const std::string &menuPath) {
        Log::Warn("[STUB] menu.navigate_to('%s') - Not yet implemented", menuPath.c_str());
        Log::Info("  Future implementation: This will navigate through menu hierarchy");
        Log::Info("  Example: 'main/play/level_select' to reach level selection");
        throw sol::error("menu.navigate_to: Not yet implemented - stub function");
    };

    // tas.menu.click_button(button_name) - Click a button by name
    menu["click_button"] = [](const std::string &buttonName) {
        Log::Warn("[STUB] menu.click_button('%s') - Not yet implemented", buttonName.c_str());
        Log::Info("  Future implementation: This will click a menu button");
        throw sol::error("menu.click_button: Not yet implemented - stub function");
    };

    // tas.menu.select_level(level_name) - Select a level from menu
    menu["select_level"] = [](const std::string &levelName) {
        Log::Warn("[STUB] menu.select_level('%s') - Not yet implemented", levelName.c_str());
        Log::Info("  Future implementation: This will select and start the specified level");
        throw sol::error("menu.select_level: Not yet implemented - stub function");
    };

    // tas.menu.go_back() - Go back to previous menu
    menu["go_back"] = []() {
        Log::Warn("[STUB] menu.go_back() - Not yet implemented");
        Log::Info("  Future implementation: This will navigate to the previous menu");
        throw sol::error("menu.go_back: Not yet implemented - stub function");
    };

    // tas.menu.go_to_main() - Go to main menu
    menu["go_to_main"] = []() {
        Log::Warn("[STUB] menu.go_to_main() - Not yet implemented");
        Log::Info("  Future implementation: This will navigate to the main menu");
        throw sol::error("menu.go_to_main: Not yet implemented - stub function");
    };

    // ===================================================================
    // Input Simulation Functions (Stub Implementation)
    // ===================================================================

    // tas.menu.send_key(key, duration) - Send a key press in menu
    menu["send_key"] = [](const std::string &key, sol::optional<int> duration) {
        Log::Warn("[STUB] menu.send_key('%s', %d) - Not yet implemented",
                                   key.c_str(), duration.value_or(1));
        Log::Info("  Future implementation: This will send keyboard input to menu");
        throw sol::error("menu.send_key: Not yet implemented - stub function");
    };

    // tas.menu.press_enter() - Press Enter key
    menu["press_enter"] = []() {
        Log::Warn("[STUB] menu.press_enter() - Not yet implemented");
        Log::Info("  Future implementation: This will press Enter in the menu");
        throw sol::error("menu.press_enter: Not yet implemented - stub function");
    };

    // tas.menu.press_escape() - Press Escape key
    menu["press_escape"] = []() {
        Log::Warn("[STUB] menu.press_escape() - Not yet implemented");
        Log::Info("  Future implementation: This will press Escape in the menu");
        throw sol::error("menu.press_escape: Not yet implemented - stub function");
    };

    // ===================================================================
    // Wait Functions (Using Event System and Conditions)
    // ===================================================================

    // tas.menu.wait_for_menu(menu_name) - Wait until entering a specific menu
    menu["wait_for_menu"] = sol::yielding([context](const std::string &menuName) {
        auto *scheduler = context->GetScheduler();
        if (!scheduler) {
            throw sol::error("menu.wait_for_menu: Scheduler not available for this context");
        }

        Log::Warn("[STUB] menu.wait_for_menu('%s') - Partial implementation", menuName.c_str());
        Log::Info("  Currently just waits, does not detect specific menu");

        // Stub: Just wait for a short time
        // Future: Check actual menu state
        scheduler->YieldTicks(60); // Wait 1 second at 60fps
    });

    // tas.menu.wait_for_game_start() - Wait until game starts
    menu["wait_for_game_start"] = sol::yielding([context]() {
        auto *scheduler = context->GetScheduler();
        if (!scheduler) {
            throw sol::error("menu.wait_for_game_start: Scheduler not available for this context");
        }

        // Wait until we're in game (implemented)
        sol::function predicate = sol::make_object(context->GetLuaState(), [context]() {
            auto *g = context->GetGameInterface();
            return g && g->IsPlaying();
        });
        scheduler->YieldUntil(predicate);
    });

    // tas.menu.wait_for_menu_entry() - Wait until entering any menu
    menu["wait_for_menu_entry"] = sol::yielding([context]() {
        auto *scheduler = context->GetScheduler();
        if (!scheduler) {
            throw sol::error("menu.wait_for_menu_entry: Scheduler not available for this context");
        }

        // Wait until we're in menu (implemented)
        sol::function predicate = sol::make_object(context->GetLuaState(), [context]() {
            auto *g = context->GetGameInterface();
            return g && !g->IsPlaying();
        });
        scheduler->YieldUntil(predicate);
    });

    // ===================================================================
    // Utility Functions
    // ===================================================================

    // tas.menu.get_available_levels() - Get list of available levels
    menu["get_available_levels"] = [context]() -> sol::object {
        Log::Warn("[STUB] menu.get_available_levels() - Not yet implemented");
        Log::Info("  Future implementation: This will return a list of available levels");

        // Return a stub list for now
        auto &lua = context->GetLuaState();
        sol::table levels = lua.create_table();
        for (int i = 1; i <= 13; i++) {
            levels[i] = "Level_" + std::to_string(i < 10 ? i : i);
        }
        return levels;
    };
}
