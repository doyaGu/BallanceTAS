#include "LuaApi.h"

#include <stdexcept>

#include "Logger.h"
#include "TASEngine.h"
#include "GameInterface.h"
#include "LuaScheduler.h"
#include "ScriptContext.h"

namespace {

[[noreturn]] void ThrowMenuStubError(const char *functionName) {
    Log::Warn("[STUB] %s - Not yet implemented", functionName);
    throw sol::error(std::string(functionName) + ": Not yet implemented - stub function");
}

} // namespace

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
        ThrowMenuStubError("menu.get_current");
    };

    // tas.menu.is_at(menu_name) - Check if at a specific menu
    menu["is_at"] = [](const std::string &menuName) -> bool {
        (void) menuName;
        ThrowMenuStubError("menu.is_at");
    };

    // ===================================================================
    // Menu Navigation Functions (Stub Implementation)
    // ===================================================================

    // tas.menu.navigate_to(menu_path) - Navigate to a menu by path
    menu["navigate_to"] = [](const std::string &menuPath) {
        (void) menuPath;
        ThrowMenuStubError("menu.navigate_to");
    };

    // tas.menu.click_button(button_name) - Click a button by name
    menu["click_button"] = [](const std::string &buttonName) {
        (void) buttonName;
        ThrowMenuStubError("menu.click_button");
    };

    // tas.menu.select_level(level_name) - Select a level from menu
    menu["select_level"] = [](const std::string &levelName) {
        (void) levelName;
        ThrowMenuStubError("menu.select_level");
    };

    // tas.menu.go_back() - Go back to previous menu
    menu["go_back"] = []() {
        ThrowMenuStubError("menu.go_back");
    };

    // tas.menu.go_to_main() - Go to main menu
    menu["go_to_main"] = []() {
        ThrowMenuStubError("menu.go_to_main");
    };

    // ===================================================================
    // Input Simulation Functions (Stub Implementation)
    // ===================================================================

    // tas.menu.send_key(key, duration) - Send a key press in menu
    menu["send_key"] = [](const std::string &key, sol::optional<int> duration) {
        (void) key;
        (void) duration;
        ThrowMenuStubError("menu.send_key");
    };

    // tas.menu.press_enter() - Press Enter key
    menu["press_enter"] = []() {
        ThrowMenuStubError("menu.press_enter");
    };

    // tas.menu.press_escape() - Press Escape key
    menu["press_escape"] = []() {
        ThrowMenuStubError("menu.press_escape");
    };

    // ===================================================================
    // Wait Functions (Using Event System and Conditions)
    // ===================================================================

    // tas.menu.wait_for_menu(menu_name) - Wait until entering a specific menu
    menu["wait_for_menu"] = sol::yielding([context](const std::string &menuName) {
        (void) context;
        (void) menuName;
        ThrowMenuStubError("menu.wait_for_menu");
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
        (void) context;
        ThrowMenuStubError("menu.get_available_levels");
    };
}
