#pragma once

#include <sol/sol.hpp>

class TASEngine;

/**
 * @class LuaApi
 * @brief A static class for registering all C++ functions and types with the Lua state.
 *
 * This class provides a centralized location for all sol2 bindings. It creates
 * the global 'tas' table and populates it with functions that call into the
 * various subsystems of the TASEngine (InputSystem, GameReader, etc.).
 */
class LuaApi {
public:
    // LuaApi is a static class and should not be instantiated.
    LuaApi() = delete;

    /**
     * @brief Registers the entire 'tas' library into the given Lua state.
     * @param engine A pointer to the TASEngine instance, providing access to all subsystems.
     */
    static void Register(TASEngine *engine);

private:
    // --- Private helper methods for organized registration ---

    static void RegisterCoreApi(sol::table& tas, TASEngine *engine);
    static void RegisterInputApi(sol::table& tas, TASEngine *engine);
    static void RegisterWorldQueryApi(sol::table& tas, TASEngine *engine);
    static void RegisterConcurrencyApi(sol::table& tas, TASEngine *engine);
    static void RegisterDebugApi(sol::table& tas, TASEngine *engine);
    static void RegisterDataTypes(sol::state& lua, TASEngine *engine);
    static void RegisterDevTools(sol::table& tas, TASEngine *engine);
};