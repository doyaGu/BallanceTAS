#pragma once

#include <sol/sol.hpp>

class TASEngine;

/**
 * @class LuaApi
 * @brief A static class for registering all C++ functions and types with the Lua state.
 *
 * This class provides a centralized location for all sol2 bindings. It creates
 * the global 'tas' table and populates it with functions that call into the
 * various subsystems of the TASEngine.
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
    static void RegisterDataTypes(sol::state &lua);
    static void RegisterVxColor(sol::state &lua);
    static void RegisterVxMatrix(sol::state &lua);
    static void RegisterVxQuaternion(sol::state &lua);
    static void RegisterVxRect(sol::state &lua);
    static void RegisterVxVector(sol::state &lua);
    static void RegisterVxIntersectionDesc(sol::state &lua);
    static void RegisterCKObject(sol::state &lua);
    static void RegisterCKSceneObject(sol::state &lua);
    static void RegisterCKBeObject(sol::state &lua);
    static void RegisterCKRenderObject(sol::state &lua);
    static void RegisterCK3dEntity(sol::state &lua);
    static void RegisterCKCamera(sol::state &lua);
    static void RegisterPhysicsObject(sol::state &lua);

    static void RegisterCoreApi(sol::table &tas, TASEngine *engine);
    static void RegisterInputApi(sol::table &tas, TASEngine *engine);
    static void RegisterWorldQueryApi(sol::table &tas, TASEngine *engine);
    static void RegisterConcurrencyApi(sol::table &tas, TASEngine *engine);
    static void RegisterDebugApi(sol::table &tas, TASEngine *engine);
};
