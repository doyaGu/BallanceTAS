#include "LuaApi.h"

#include "LuaApiCommon.h"
#include "LuaApiValues.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include <CK3dEntity.h>
#include <CKCamera.h>
#include <VxMath.h>


using namespace LuaApiCommon;

static CKCamera *CheckCKCamera(lua_State *L, int index) {
    return tas::lua::CheckUserdata<CKCamera>(L, index, kCKCameraMt);
}



static int FrontPlane(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushnumber(L, camera ? camera->GetFrontPlane() : 0.0f);
    return 1;
}

static int SetFrontPlane(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    camera->SetFrontPlane(static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

static int BackPlane(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushnumber(L, camera ? camera->GetBackPlane() : 0.0f);
    return 1;
}

static int SetBackPlane(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    camera->SetBackPlane(static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

static int Fov(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushnumber(L, camera ? camera->GetFov() : 0.0f);
    return 1;
}

static int SetFov(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    camera->SetFov(static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

static int ProjectionType(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushinteger(L, camera ? static_cast<lua_Integer>(camera->GetProjectionType()) : 0);
    return 1;
}

static int SetProjectionType(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    camera->SetProjectionType(static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

static int OrthographicZoom(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushnumber(L, camera ? camera->GetOrthographicZoom() : 0.0f);
    return 1;
}

static int SetOrthographicZoom(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    camera->SetOrthographicZoom(static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

static int AspectRatio(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    int width = 0;
    int height = 0;
    if (camera) {
        camera->GetAspectRatio(width, height);
    }
    PushVx2DVector(L, Vx2DVector(static_cast<float>(width), static_cast<float>(height)));
    return 1;
}

static int SetAspectRatio(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    auto *aspect = CheckVx2DVector(L, 2);
    if (!camera || !aspect) {
        return luaL_error(L, "CKCamera.aspect_ratio: expected a Vx2DVector");
    }
    camera->SetAspectRatio(static_cast<int>(aspect->x), static_cast<int>(aspect->y));
    return 0;
}

static int ComputeProjectionMatrix(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    VxMatrix matrix;
    camera->ComputeProjectionMatrix(matrix);
    PushVxMatrix(L, matrix);
    return 1;
}

static int ResetRoll(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (camera) {
        camera->ResetRoll();
    }
    return 0;
}

static int Roll(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (camera) {
        camera->Roll(static_cast<float>(luaL_checknumber(L, 2)));
    }
    return 0;
}

static int Target(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    PushCK3dEntity(L, camera ? camera->GetTarget() : nullptr);
    return 1;
}

static int SetTarget(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    camera->SetTarget(OptCK3dEntity(L, 2));
    return 0;
}

static int ToString(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushfstring(L, "CKCamera(id=%d, name=%s)",
                    camera ? camera->GetID() : 0,
                    camera && camera->GetName() ? camera->GetName() : "");
    return 1;
}

void LuaApi::RegisterCKCamera(lua_State *state) {
    tas::lua::LuaStackGuard guard(state);

    tas::lua::LuaUserdataRegistry<CKCamera>(state, kCKCameraMt)
        .Base<CK3dEntity>(kCK3dEntityMt)
        .Property("front_plane", FrontPlane, SetFrontPlane)
        .Property("back_plane", BackPlane, SetBackPlane)
        .Property("fov", Fov, SetFov)
        .Property("projection_type", ProjectionType, SetProjectionType)
        .Property("orthographic_zoom", OrthographicZoom, SetOrthographicZoom)
        .Property("aspect_ratio", AspectRatio, SetAspectRatio)
        .Property("target", Target, SetTarget)
        .Method("compute_projection_matrix", ComputeProjectionMatrix)
        .Method("reset_roll", ResetRoll)
        .Method("roll", Roll)
        .MetaMethod("__tostring", ToString);
}
