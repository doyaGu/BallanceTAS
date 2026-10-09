#include "LuaApi.h"

#include "LuaApiCommon.h"
#include "LuaApiValues.h"
#include "LuaRuntime/LuaStackGuard.h"

#include "GameInterface.h"
#include "ScriptContext.h"
#include "physics_RT.h"

#include <CK3dEntity.h>
#include <CKCamera.h>

using namespace LuaApiCommon;

static GameInterface *RequireGame(lua_State *L, const char *where) {
    ScriptContext *context = GetContext(L);
    GameInterface *game = context ? context->GetGameInterface() : nullptr;
    if (!game) {
        Raise(L, where, "the game interface is not available");
    }
    return game;
}

static int GetPoints(lua_State *L) {
    lua_pushinteger(L, RequireGame(L, "tas.get_points")->GetPoints());
    return 1;
}

static int GetLifeCount(lua_State *L) {
    lua_pushinteger(L, RequireGame(L, "tas.get_life_count")->GetLifeCount());
    return 1;
}

// tas.get_sr_score() -> seconds
static int GetSRScore(lua_State *L) {
    lua_pushnumber(L, RequireGame(L, "tas.get_sr_score")->GetSRScore());
    return 1;
}

static int GetHSScore(lua_State *L) {
    lua_pushinteger(L, RequireGame(L, "tas.get_hs_score")->GetHSScore());
    return 1;
}

// tas.get_object_by_id(id) -> CK3dEntity | nil
static int GetObjectById(lua_State *L) {
    const lua_Integer id = luaL_checkinteger(L, 1);
    GameInterface *game = RequireGame(L, "tas.get_object_by_id");
    PushCK3dEntity(L, id > 0 ? game->GetObjectByID(static_cast<int>(id)) : nullptr);
    return 1;
}

// tas.get_object(name) -> CK3dEntity | nil
static int GetObject(lua_State *L) {
    const char *name = CheckNonEmptyString(L, 1, "tas.get_object");
    PushCK3dEntity(L, RequireGame(L, "tas.get_object")->GetObjectByName(name));
    return 1;
}

static int GetCamera(lua_State *L) {
    PushCKCamera(L, RequireGame(L, "tas.get_camera")->GetActiveCamera());
    return 1;
}

static int GetBall(lua_State *L) {
    PushCK3dEntity(L, RequireGame(L, "tas.get_ball")->GetActiveBall());
    return 1;
}

static PhysicsObject *BallPhysicsObject(lua_State *L, const char *where) {
    GameInterface *game = RequireGame(L, where);
    CK3dEntity *ball = game->GetActiveBall();
    return ball ? game->GetPhysicsObject(ball) : nullptr;
}

// tas.get_physics_object(entity) -> PhysicsObject | nil (not physicalized)
static int GetPhysicsObject(lua_State *L) {
    CK3dEntity *entity = CheckCK3dEntity(L, 1);
    PushPhysicsObject(L, RequireGame(L, "tas.get_physics_object")->GetPhysicsObject(entity));
    return 1;
}

static int GetBallPosition(lua_State *L) {
    GameInterface *game = RequireGame(L, "tas.get_ball_position");
    CK3dEntity *ball = game->GetActiveBall();
    if (!ball) {
        lua_pushnil(L);
        return 1;
    }
    PushVxVector(L, game->GetPosition(ball));
    return 1;
}

static int GetBallVelocity(lua_State *L) {
    PhysicsObject *object = BallPhysicsObject(L, "tas.get_ball_velocity");
    if (!object) {
        lua_pushnil(L);
        return 1;
    }
    VxVector velocity;
    object->GetVelocity(&velocity, nullptr);
    PushVxVector(L, velocity);
    return 1;
}

static int GetBallAngularVelocity(lua_State *L) {
    PhysicsObject *object = BallPhysicsObject(L, "tas.get_ball_angular_velocity");
    if (!object) {
        lua_pushnil(L);
        return 1;
    }
    VxVector angularVelocity;
    object->GetVelocity(nullptr, &angularVelocity);
    PushVxVector(L, angularVelocity);
    return 1;
}

void LuaApi::RegisterWorldQueryApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);
    lua_getglobal(state, "tas");
    // tas.is_paused / is_playing / get_level / get_sector are aliases set up by RegisterLevelApi.
    SetClosure(state, "get_points", GetPoints, context);
    SetClosure(state, "get_life_count", GetLifeCount, context);
    SetClosure(state, "get_sr_score", GetSRScore, context);
    SetClosure(state, "get_hs_score", GetHSScore, context);
    SetClosure(state, "get_object", GetObject, context);
    SetClosure(state, "get_object_by_id", GetObjectById, context);
    SetClosure(state, "get_camera", GetCamera, context);
    SetClosure(state, "get_ball", GetBall, context);
    SetClosure(state, "get_ball_position", GetBallPosition, context);
    SetClosure(state, "get_ball_velocity", GetBallVelocity, context);
    SetClosure(state, "get_ball_angular_velocity", GetBallAngularVelocity, context);
    SetClosure(state, "get_physics_object", GetPhysicsObject, context);
    lua_pop(state, 1);
}
