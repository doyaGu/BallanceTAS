#include "LuaApi.h"

#include "TASEngine.h"
#include "ScriptContext.h"

// Register APIs for a specific ScriptContext
// Uses context's local subsystems for complete isolation between contexts
void LuaApi::Register(ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::Register(context): context cannot be null");
    }

    sol::state &lua = context->GetLuaState();

    // Create the main 'tas' table in the Lua global scope.
    sol::table tas_table = lua.create_table();
    lua["tas"] = tas_table;

    // Register all the sub-modules of the API using context's local subsystems
    RegisterDataTypes(lua);
    RegisterCoreApi(tas_table, context);
    RegisterInputApi(tas_table, context);
    RegisterWorldQueryApi(tas_table, context);
    RegisterConcurrencyApi(tas_table, context);
    RegisterEventApi(tas_table, context);
    RegisterDebugApi(tas_table, context);
    RegisterRecordApi(tas_table, context);
    RegisterProjectApi(tas_table, context);
    RegisterLevelApi(tas_table, context);
    RegisterMenuApi(tas_table, context);
    RegisterGlobalApi(tas_table, context);
    RegisterContextCommunicationApi(tas_table, context);
    RegisterGCApi(tas_table, context);
    RegisterSharedBufferApi(tas_table, context);
    RegisterResultApi(tas_table, context);
    RegisterAsyncApi(tas_table, context);
}

void LuaApi::AddLuaPath(sol::state &lua, const std::string &path) {
    if (path.empty()) {
        return;
    }
    std::string currentPath = lua["package"]["path"];
    std::string newPath;
    char sep = path.back();
    if (sep == '/') {
        newPath = path + "?.lua;" + path + "?/init.lua;" + currentPath;
    } else if (sep == '\\') {
        newPath = path + "?.lua;" + path + "?\\init.lua;" + currentPath;
    } else {
        newPath = path + "\\?.lua;" + path + "\\?\\init.lua;" + currentPath;
    }
    lua["package"]["path"] = newPath;
}

void LuaApi::RegisterDataTypes(sol::state &lua) {
    RegisterVxColor(lua);
    RegisterVxMatrix(lua);
    RegisterVxQuaternion(lua);
    RegisterVxRect(lua);
    RegisterVxVector(lua);
    RegisterVxIntersectionDesc(lua);

    RegisterCKEnums(lua);
    RegisterCKObject(lua);
    RegisterCKSceneObject(lua);
    RegisterCKBeObject(lua);
    RegisterCKRenderObject(lua);
    RegisterCK3dEntity(lua);
    RegisterCKCamera(lua);

    RegisterPhysicsObject(lua);
}
