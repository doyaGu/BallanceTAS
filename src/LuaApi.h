#pragma once

#include <sol/sol.hpp>

class ScriptContext;
class TASEngine;

/**
 * @class LuaApi
 * @brief A static class for registering all C++ functions and types with the Lua state.
 *
 * This class provides a centralized location for all sol2 bindings. It creates
 * the global 'tas' table and populates it with functions that call into the
 * various subsystems of the TASEngine.
 *
 * The Register function requires a specific ScriptContext to ensure proper context
 * isolation in the multi-context system.
 */
class LuaApi {
public:
    // LuaApi is a static class and should not be instantiated.
    LuaApi() = delete;

    /**
     * @brief Registers the 'tas' library for a specific script context.
     * Uses context's local subsystems (Lua VM, Scheduler, EventManager, InputSystem)
     * for complete isolation between contexts.
     * @param context A pointer to the ScriptContext instance for context-local systems.
     */
    static void Register(ScriptContext *context);

    static void AddLuaPath(sol::state &lua, const std::string &path);

private:
    // --- Private helper methods for organized registration ---
    static void RegisterDataTypes(sol::state &lua);
    static void RegisterVxColor(sol::state &lua);
    static void RegisterVxMatrix(sol::state &lua);
    static void RegisterVxQuaternion(sol::state &lua);
    static void RegisterVxRect(sol::state &lua);
    static void RegisterVxVector(sol::state &lua);
    static void RegisterVxIntersectionDesc(sol::state &lua);
    static void RegisterCKEnums(sol::state &lua);
    static void RegisterCKObject(sol::state &lua);
    static void RegisterCKSceneObject(sol::state &lua);
    static void RegisterCKBeObject(sol::state &lua);
    static void RegisterCKRenderObject(sol::state &lua);
    static void RegisterCK3dEntity(sol::state &lua);
    static void RegisterCKCamera(sol::state &lua);
    static void RegisterPhysicsObject(sol::state &lua);

    // Context-aware API registration methods
    // Uses context's local subsystems for proper isolation between contexts
    static void RegisterCoreApi(sol::table &tas, ScriptContext *context);
    static void RegisterInputApi(sol::table &tas, ScriptContext *context);
    static void RegisterWorldQueryApi(sol::table &tas, ScriptContext *context);
    static void RegisterConcurrencyApi(sol::table &tas, ScriptContext *context);
    static void RegisterEventApi(sol::table &tas, ScriptContext *context);
    static void RegisterDebugApi(sol::table &tas, ScriptContext *context);
    static void RegisterRecordApi(sol::table &tas, ScriptContext *context);
    static void RegisterProjectApi(sol::table &tas, ScriptContext *context);
    static void RegisterLevelApi(sol::table &tas, ScriptContext *context);
    static void RegisterMenuApi(sol::table &tas, ScriptContext *context);
    static void RegisterGlobalApi(sol::table &tas, ScriptContext *context);
    static void RegisterContextCommunicationApi(sol::table &tas, ScriptContext *context);
    static void RegisterGCApi(sol::table &tas, ScriptContext *context);
    static void RegisterSharedBufferApi(sol::table &tas, ScriptContext *context);
    static void RegisterResultApi(sol::table &tas, ScriptContext *context);
    static void RegisterAsyncApi(sol::table &tas, ScriptContext *context);
};
