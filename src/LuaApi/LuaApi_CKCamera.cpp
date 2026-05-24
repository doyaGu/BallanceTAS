#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include <CK3dEntity.h>
#include <CKCamera.h>
#include <VxMath.h>

namespace {

constexpr const char *kCKCameraMt = "BallanceTAS.CKCamera";
constexpr const char *kCK3dEntityMt = "BallanceTAS.CK3dEntity";
constexpr const char *kVx2DVectorMt = "BallanceTAS.Vx2DVector";
constexpr const char *kVxMatrixMt = "BallanceTAS.VxMatrix";

CKCamera *CheckCKCamera(lua_State *L, int index) {
    return tas::lua::CheckUserdata<CKCamera>(L, index, kCKCameraMt);
}

Vx2DVector *CheckVx2DVector(lua_State *L, int index) {
    return tas::lua::CheckUserdata<Vx2DVector>(L, index, kVx2DVectorMt);
}

CK3dEntity *OptionalCK3dEntity(lua_State *L, int index) {
    if (lua_isnoneornil(L, index)) {
        return nullptr;
    }
    return tas::lua::CheckUserdata<CK3dEntity>(L, index, kCK3dEntityMt);
}

void PushCK3dEntity(lua_State *L, CK3dEntity *entity) {
    if (!entity) {
        lua_pushnil(L);
        return;
    }
    tas::lua::PushBorrowedUserdata<CK3dEntity>(L, kCK3dEntityMt, entity);
}

void PushVx2DVector(lua_State *L, float x, float y) {
    lua_getglobal(L, "Vx2DVector");
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_call(L, 2, 1);
}

void PushVxVector(lua_State *L, const VxVector &value) {
    lua_getglobal(L, "VxVector");
    lua_pushnumber(L, value.x);
    lua_pushnumber(L, value.y);
    lua_pushnumber(L, value.z);
    lua_call(L, 3, 1);
}

int CameraName(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushstring(L, camera && camera->GetName() ? camera->GetName() : "");
    return 1;
}

int CameraSetName(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    const char *name = luaL_checkstring(L, 2);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    camera->SetName(const_cast<char *>(name));
    return 0;
}

int CameraId(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushinteger(L, camera ? camera->GetID() : 0);
    return 1;
}

int CameraClassId(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushinteger(L, camera ? camera->GetClassID() : 0);
    return 1;
}

int CameraGetPosition(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    VxVector position;
    if (camera) {
        camera->GetPosition(&position);
    } else {
        position.x = position.y = position.z = 0.0f;
    }
    PushVxVector(L, position);
    return 1;
}

int CameraGetScale(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    VxVector scale;
    if (camera) {
        camera->GetScale(&scale);
    } else {
        scale.x = scale.y = scale.z = 0.0f;
    }
    PushVxVector(L, scale);
    return 1;
}

int FrontPlane(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushnumber(L, camera ? camera->GetFrontPlane() : 0.0f);
    return 1;
}

int SetFrontPlane(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    camera->SetFrontPlane(static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

int BackPlane(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushnumber(L, camera ? camera->GetBackPlane() : 0.0f);
    return 1;
}

int SetBackPlane(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    camera->SetBackPlane(static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

int Fov(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushnumber(L, camera ? camera->GetFov() : 0.0f);
    return 1;
}

int SetFov(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    camera->SetFov(static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

int ProjectionType(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushinteger(L, camera ? static_cast<lua_Integer>(camera->GetProjectionType()) : 0);
    return 1;
}

int SetProjectionType(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    camera->SetProjectionType(static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

int OrthographicZoom(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushnumber(L, camera ? camera->GetOrthographicZoom() : 0.0f);
    return 1;
}

int SetOrthographicZoom(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    camera->SetOrthographicZoom(static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

int AspectRatio(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    int width = 0;
    int height = 0;
    if (camera) {
        camera->GetAspectRatio(width, height);
    }
    PushVx2DVector(L, static_cast<float>(width), static_cast<float>(height));
    return 1;
}

int SetAspectRatio(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    auto *aspect = CheckVx2DVector(L, 2);
    if (!camera || !aspect) {
        return luaL_error(L, "CKCamera aspect_ratio requires camera and Vx2DVector");
    }
    camera->SetAspectRatio(static_cast<int>(aspect->x), static_cast<int>(aspect->y));
    return 0;
}

int ComputeProjectionMatrix(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    VxMatrix matrix;
    camera->ComputeProjectionMatrix(matrix);
    tas::lua::PushOwnedUserdata<VxMatrix>(L, kVxMatrixMt, matrix);
    return 1;
}

int ResetRoll(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (camera) {
        camera->ResetRoll();
    }
    return 0;
}

int Roll(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (camera) {
        camera->Roll(static_cast<float>(luaL_checknumber(L, 2)));
    }
    return 0;
}

int Target(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    PushCK3dEntity(L, camera ? camera->GetTarget() : nullptr);
    return 1;
}

int SetTarget(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    if (!camera) {
        return luaL_error(L, "CKCamera is null");
    }
    camera->SetTarget(OptionalCK3dEntity(L, 2));
    return 0;
}

int ToString(lua_State *L) {
    auto *camera = CheckCKCamera(L, 1);
    lua_pushfstring(L, "CKCamera(id=%d, name=%s)",
                    camera ? camera->GetID() : 0,
                    camera && camera->GetName() ? camera->GetName() : "");
    return 1;
}

} // namespace

void LuaApi::RegisterCKCamera(lua_State *state) {
    tas::lua::LuaStackGuard guard(state);

    tas::lua::LuaUserdataRegistry<CKCamera>(state, kCKCameraMt)
        .Base<CK3dEntity>(kCK3dEntityMt)
        .Property("name", CameraName, CameraSetName)
        .ReadonlyProperty("id", CameraId)
        .ReadonlyProperty("class_id", CameraClassId)
        .Property("front_plane", FrontPlane, SetFrontPlane)
        .Property("back_plane", BackPlane, SetBackPlane)
        .Property("fov", Fov, SetFov)
        .Property("projection_type", ProjectionType, SetProjectionType)
        .Property("orthographic_zoom", OrthographicZoom, SetOrthographicZoom)
        .Property("aspect_ratio", AspectRatio, SetAspectRatio)
        .Property("target", Target, SetTarget)
        .Method("get_position", CameraGetPosition)
        .Method("get_scale", CameraGetScale)
        .Method("compute_projection_matrix", ComputeProjectionMatrix)
        .Method("reset_roll", ResetRoll)
        .Method("roll", Roll)
        .MetaMethod("__tostring", ToString);
}
