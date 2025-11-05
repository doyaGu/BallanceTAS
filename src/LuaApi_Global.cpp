#include "LuaApi.h"

#include <stdexcept>
#include <chrono>

#include "TASEngine.h"
#include "ScriptContext.h"

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
