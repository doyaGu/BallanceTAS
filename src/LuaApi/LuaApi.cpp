#include "LuaApi.h"

#include <stdexcept>

#include "ScriptContext.h"

namespace {

void EnsureTasTable(lua_State *state) {
    lua_getglobal(state, "tas");
    if (lua_istable(state, -1)) {
        lua_pop(state, 1);
        return;
    }
    lua_pop(state, 1);
    lua_newtable(state);
    lua_setglobal(state, "tas");
}

} // namespace

void LuaApi::Register(ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::Register(context): context cannot be null");
    }

    lua_State *state = context->GetLuaState().Get();
    EnsureTasTable(state);

    RegisterVxVector(state);
    RegisterVxColor(state);
    RegisterVxRect(state);
    RegisterVxQuaternion(state);
    RegisterVxMatrix(state);
    RegisterVxIntersectionDesc(state);
    RegisterCKEnums(state);
    RegisterCKObject(state);
    RegisterCKSceneObject(state);
    RegisterCKBeObject(state);
    RegisterCKRenderObject(state);
    RegisterCK3dEntity(state);
    RegisterCKCamera(state);
    RegisterPhysicsObject(state);
    RegisterCoreApi(state, context);
    RegisterInputApi(state, context);
    RegisterConcurrencyApi(state, context);
    RegisterAsyncApi(state, context);
    RegisterContextCommunicationApi(state, context);
    RegisterWorldQueryApi(state, context);
    RegisterDeterminismApi(state, context);
    RegisterSavestateApi(state, context);
    RegisterGCApi(state, context);
    RegisterDebugApi(state, context);
    RegisterResultApi(state, context);
    RegisterProjectApi(state, context);
    RegisterRecordApi(state, context);
    RegisterLevelApi(state, context);
    RegisterGlobalApi(state, context);
    RegisterMenuApi(state, context);
    RegisterSharedBufferApi(state, context);
}

void LuaApi::AddLuaPath(lua_State *state, const std::string &path) {
    if (!state || path.empty()) {
        return;
    }

    lua_getglobal(state, "package");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        return;
    }

    lua_getfield(state, -1, "path");
    const char *currentPath = lua_tostring(state, -1);
    std::string newPath = currentPath ? currentPath : "";
    lua_pop(state, 1);

    const char last = path.back();
    if (last == '/' || last == '\\') {
        newPath = path + "?.lua;" + path + "?/init.lua;" + newPath;
    } else {
        newPath = path + "\\?.lua;" + path + "\\?\\init.lua;" + newPath;
    }

    lua_pushlstring(state, newPath.data(), newPath.size());
    lua_setfield(state, -2, "path");
    lua_pop(state, 1);
}
