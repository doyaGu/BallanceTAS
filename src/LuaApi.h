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
    static void RegisterDataTypes(sol::state &lua);
    static void RegisterVxColor(sol::state &lua);
    static void RegisterVxMatrix(sol::state &lua);
    static void RegisterVxQuaternion(sol::state &lua);
    static void RegisterVxRect(sol::state &lua);
    static void RegisterVxVector(sol::state &lua);
    static void RegisterCKObject(sol::state &lua);
    // static void RegisterCKParameterIn(sol::state &lua);
    // static void RegisterCKParameter(sol::state &lua);
    // static void RegisterCKParameterOut(sol::state &lua);
    // static void RegisterCKParameterLocal(sol::state &lua);
    // static void RegisterCKParameterOperation(sol::state &lua);
    // static void RegisterCKBehaviorLink(sol::state &lua);
    // static void RegisterCKBehaviorIO(sol::state &lua);
    // static void RegisterCKRenderContext(sol::state &lua);
    // static void RegisterCKKinematicChain(sol::state &lua);
    // static void RegisterCKLayer(sol::state &lua);
    static void RegisterCKSceneObject(sol::state &lua);
    // static void RegisterCKBehavior(sol::state &lua);
    // static void RegisterCKObjectAnimation(sol::state &lua);
    // static void RegisterCKAnimation(sol::state &lua);
    // static void RegisterCKKeyedAnimation(sol::state &lua);
    static void RegisterCKBeObject(sol::state &lua);
    static void RegisterCKScene(sol::state &lua);
    static void RegisterCKLevel(sol::state &lua);
    static void RegisterCKGroup(sol::state &lua);
    static void RegisterCKMaterial(sol::state &lua);
    static void RegisterCKTexture(sol::state &lua);
    static void RegisterCKMesh(sol::state &lua);
    // static void RegisterCKPatchMesh(sol::state &lua);
    // static void RegisterCKDataArray(sol::state &lua);
    // static void RegisterCKSound(sol::state &lua);
    // static void RegisterCKWaveSound(sol::state &lua);
    // static void RegisterCKMidiSound(sol::state &lua);
    static void RegisterCKRenderObject(sol::state &lua);
    static void RegisterCK2dEntity(sol::state &lua);
    // static void RegisterCKSprite(sol::state &lua);
    // static void RegisterCKSpriteText(sol::state &lua);
    static void RegisterCK3dEntity(sol::state &lua);
    static void RegisterCKCamera(sol::state &lua);
    static void RegisterCKTargetCamera(sol::state &lua);
    static void RegisterCKPlace(sol::state &lua);
    // static void RegisterCKCurvePoint(sol::state &lua);
    // static void RegisterCKSprite3D(sol::state &lua);
    static void RegisterCKLight(sol::state &lua);
    static void RegisterCKTargetLight(sol::state &lua);
    // static void RegisterCK3dObject(sol::state &lua);
    // static void RegisterCKCurve(sol::state &lua);
    // static void RegisterCKGrid(sol::state &lua);

    static void RegisterCoreApi(sol::table &tas, TASEngine *engine);
    static void RegisterInputApi(sol::table &tas, TASEngine *engine);
    static void RegisterWorldQueryApi(sol::table &tas, TASEngine *engine);
    static void RegisterConcurrencyApi(sol::table &tas, TASEngine *engine);
    static void RegisterDebugApi(sol::table &tas, TASEngine *engine);
    static void RegisterDevTools(sol::table &tas, TASEngine *engine);
};
