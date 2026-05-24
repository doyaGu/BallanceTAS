#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include "GameInterface.h"
#include "ScriptContext.h"

#include <CK3dEntity.h>
#include <CKCamera.h>
#include <CKObject.h>
#include <VxMath.h>

constexpr const char *kCKObjectMt = "BallanceTAS.CKObject";
constexpr const char *kCK3dEntityMt = "BallanceTAS.CK3dEntity";
constexpr const char *kCKCameraMt = "BallanceTAS.CKCamera";
constexpr const char *kContextUpvalue = "LuaApi.World.Context";

static ScriptContext *GetContext(lua_State *L) {
    return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

static void PushCKObject(lua_State *L, CKObject *object) {
    if (!object) {
        lua_pushnil(L);
        return;
    }
    tas::lua::PushBorrowedUserdata<CKObject>(L, kCKObjectMt, object);
}

static void PushCK3dEntity(lua_State *L, CK3dEntity *entity) {
    if (!entity) {
        lua_pushnil(L);
        return;
    }
    tas::lua::PushBorrowedUserdata<CK3dEntity>(L, kCK3dEntityMt, entity);
}

static void PushCKCamera(lua_State *L, CKCamera *camera) {
    if (!camera) {
        lua_pushnil(L);
        return;
    }
    tas::lua::PushBorrowedUserdata<CKCamera>(L, kCKCameraMt, camera);
}

static void SetContextFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, function, 1);
    lua_setfield(L, -2, name);
}

static int IsPaused(lua_State *L) {
    auto *context = GetContext(L);
    const auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushboolean(L, game && game->IsPaused());
    return 1;
}

static int IsPlaying(lua_State *L) {
    auto *context = GetContext(L);
    const auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushboolean(L, game && game->IsPlaying());
    return 1;
}

static int GetLevel(lua_State *L) {
    auto *context = GetContext(L);
    const auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushinteger(L, game ? game->GetCurrentLevel() : 0);
    return 1;
}

static int GetSector(lua_State *L) {
    auto *context = GetContext(L);
    const auto *game = context ? context->GetGameInterface() : nullptr;
    lua_pushinteger(L, game ? game->GetCurrentSector() : 0);
    return 1;
}

static int GetObjectById(lua_State *L) {
    auto *context = GetContext(L);
    const auto *game = context ? context->GetGameInterface() : nullptr;
    const int id = static_cast<int>(luaL_checkinteger(L, 1));
    PushCK3dEntity(L, game && id > 0 ? game->GetObjectByID(id) : nullptr);
    return 1;
}

static int GetObject(lua_State *L) {
    auto *context = GetContext(L);
    const auto *game = context ? context->GetGameInterface() : nullptr;
    const char *name = luaL_checkstring(L, 1);
    PushCK3dEntity(L, game && name && *name ? game->GetObjectByName(name) : nullptr);
    return 1;
}

static int GetCamera(lua_State *L) {
    auto *context = GetContext(L);
    const auto *game = context ? context->GetGameInterface() : nullptr;
    PushCKCamera(L, game ? game->GetActiveCamera() : nullptr);
    return 1;
}

static int GetBall(lua_State *L) {
    auto *context = GetContext(L);
    const auto *game = context ? context->GetGameInterface() : nullptr;
    PushCK3dEntity(L, game ? game->GetActiveBall() : nullptr);
    return 1;
}

static int GetBallPosition(lua_State *L) {
    auto *context = GetContext(L);
    const auto *game = context ? context->GetGameInterface() : nullptr;
    CK3dEntity *ball = game ? game->GetActiveBall() : nullptr;
    if (!game || !ball) {
        lua_pushnil(L);
        return 1;
    }
    VxVector position = game->GetPosition(ball);
    lua_getglobal(L, "VxVector");
    lua_pushnumber(L, position.x);
    lua_pushnumber(L, position.y);
    lua_pushnumber(L, position.z);
    lua_call(L, 3, 1);
    return 1;
}

void LuaApi::RegisterWorldQueryApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);

    lua_getglobal(state, "tas");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_setglobal(state, "tas");
        lua_getglobal(state, "tas");
    }

    lua_pushlightuserdata(state, context);
    lua_setfield(state, LUA_REGISTRYINDEX, kContextUpvalue);

    SetContextFunction(state, "is_paused", IsPaused, context);
    SetContextFunction(state, "is_playing", IsPlaying, context);
    SetContextFunction(state, "get_level", GetLevel, context);
    SetContextFunction(state, "get_sector", GetSector, context);
    SetContextFunction(state, "get_object", GetObject, context);
    SetContextFunction(state, "get_object_by_id", GetObjectById, context);
    SetContextFunction(state, "get_camera", GetCamera, context);
    SetContextFunction(state, "get_ball", GetBall, context);
    SetContextFunction(state, "get_ball_position", GetBallPosition, context);

    lua_pop(state, 1);
}
