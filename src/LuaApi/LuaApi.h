#pragma once

#include <string>

#include "LuaRuntime/LuaHeaders.h"

class ScriptContext;
class LuaScheduler;

class LuaApi {
public:
    LuaApi() = delete;

    static void Register(ScriptContext *context);
    static void AddLuaPath(lua_State *state, const std::string &path);
    static void RegisterVxVector(lua_State *state);
    static void RegisterVxColor(lua_State *state);
    static void RegisterVxRect(lua_State *state);
    static void RegisterVxQuaternion(lua_State *state);
    static void RegisterVxMatrix(lua_State *state);
    static void RegisterSharedBufferApi(lua_State *state, ScriptContext *context);
    static void RegisterConcurrencyApi(lua_State *state, ScriptContext *context, LuaScheduler *scheduler = nullptr);
    static void RegisterAsyncApi(lua_State *state, ScriptContext *context, LuaScheduler *scheduler = nullptr);

private:
    static void RegisterCoreApi(lua_State *state, ScriptContext *context);
    static void RegisterInputApi(lua_State *state, ScriptContext *context);
    static void RegisterContextCommunicationApi(lua_State *state, ScriptContext *context);
    static void RegisterVxIntersectionDesc(lua_State *state);
    static void RegisterCKObject(lua_State *state);
    static void RegisterCKSceneObject(lua_State *state);
    static void RegisterCKBeObject(lua_State *state);
    static void RegisterCKRenderObject(lua_State *state);
    static void RegisterCK3dEntity(lua_State *state);
    static void RegisterCKCamera(lua_State *state);
    static void RegisterPhysicsObject(lua_State *state);
    static void RegisterCKEnums(lua_State *state);
    static void RegisterWorldQueryApi(lua_State *state, ScriptContext *context);
    static void RegisterDeterminismApi(lua_State *state, ScriptContext *context);
    static void RegisterSavestateApi(lua_State *state, ScriptContext *context);
    static void RegisterGCApi(lua_State *state, ScriptContext *context);
    static void RegisterDebugApi(lua_State *state, ScriptContext *context);
    static void RegisterResultApi(lua_State *state, ScriptContext *context);
    static void RegisterProjectApi(lua_State *state, ScriptContext *context);
    static void RegisterRecordApi(lua_State *state, ScriptContext *context);
    static void RegisterLevelApi(lua_State *state, ScriptContext *context);
    static void RegisterGlobalApi(lua_State *state, ScriptContext *context);
    static void RegisterMenuApi(lua_State *state, ScriptContext *context);
};
