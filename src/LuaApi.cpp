#include "LuaApi.h"

#include "TASEngine.h"

void LuaApi::Register(TASEngine *engine) {
    sol::state &lua = engine->GetLuaState();

    // Create the main 'tas' table in the Lua global scope.
    sol::table tas_table = lua.create_table();
    lua["tas"] = tas_table;

    // Register all the sub-modules of the API.
    RegisterDataTypes(lua);
    RegisterCoreApi(tas_table, engine);
    RegisterInputApi(tas_table, engine);
    RegisterWorldQueryApi(tas_table, engine);
    RegisterConcurrencyApi(tas_table, engine);
    RegisterDebugApi(tas_table, engine);
}

void LuaApi::RegisterDataTypes(sol::state &lua) {
    RegisterVxColor(lua);
    RegisterVxMatrix(lua);
    RegisterVxQuaternion(lua);
    RegisterVxRect(lua);
    RegisterVxVector(lua);
    RegisterVxIntersectionDesc(lua);

    RegisterCKObject(lua);
    RegisterCKSceneObject(lua);
    RegisterCKBeObject(lua);
    RegisterCKRenderObject(lua);
    RegisterCK3dEntity(lua);
    RegisterCKCamera(lua);

    RegisterPhysicsObject(lua);
}
