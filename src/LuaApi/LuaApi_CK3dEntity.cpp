#include "LuaApi.h"

#include "LuaApiCommon.h"
#include "LuaApiValues.h"
#include "LuaRuntime/LuaStackGuard.h"
#include "LuaRuntime/LuaUserdata.h"

#include "GameInterface.h"
#include "ScriptContext.h"

#include <CK3dEntity.h>
#include <CKRenderObject.h>
#include <cstdio>
#include <VxMath.h>

using namespace LuaApiCommon;

static int CK3dEntityChildrenCount(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    lua_pushinteger(L, entity->GetChildrenCount());
    return 1;
}

static int CK3dEntityFlags(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    lua_pushinteger(L, entity->GetFlags());
    return 1;
}

static int CK3dEntityRadius(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    lua_pushnumber(L, entity->GetRadius());
    return 1;
}

static int CK3dEntityParent(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    PushCK3dEntity(L, entity->GetParent());
    return 1;
}

static int CK3dEntityGetChild(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    const int index = static_cast<int>(luaL_checkinteger(L, 2));
    PushCK3dEntity(L, index >= 0 && index < entity->GetChildrenCount() ? entity->GetChild(index) : nullptr);
    return 1;
}

static int CK3dEntityGetScale(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    VxVector scale;
    entity->GetScale(&scale);
    PushVxVector(L, scale);
    return 1;
}

// Moving the render entity of a physicalized object does nothing: physics overwrites it
// on the next frame. The shipped physics_RT.dll has no teleport, so refuse instead.
static CK3dEntity *CheckMovableEntity(lua_State *L, int index, const char *method) {
    CK3dEntity *entity = CheckCK3dEntity(L, index);
    ScriptContext *context = GetRegistryContext(L);
    const auto *game = context ? context->GetGameInterface() : nullptr;
    if (game && game->GetPhysicsObject(entity)) {
        luaL_error(L, "CK3dEntity:%s: '%s' is physicalized; use its PhysicsObject (tas.get_physics_object) instead",
                   method, entity->GetName() ? entity->GetName() : "?");
    }
    return entity;
}

static int CK3dEntityGetPositionIn(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    VxVector position;
    entity->GetPosition(&position, OptCK3dEntity(L, 2));
    PushVxVector(L, position);
    return 1;
}

// entity:set_position(pos[, ref[, keep_children]])
static int CK3dEntitySetPosition(lua_State *L) {
    CheckMovableEntity(L, 1, "set_position")->SetPosition(CheckVxVector(L, 2), OptCK3dEntity(L, 3), (OptBool(L, 4, false) ? TRUE : FALSE));
    return 0;
}

static int CK3dEntityTranslate(lua_State *L) {
    CheckMovableEntity(L, 1, "translate")->Translate(CheckVxVector(L, 2), OptCK3dEntity(L, 3), (OptBool(L, 4, false) ? TRUE : FALSE));
    return 0;
}

// entity:rotate(axis, angle_radians[, ref[, keep_children]])
static int CK3dEntityRotate(lua_State *L) {
    auto *entity = CheckMovableEntity(L, 1, "rotate");
    const VxVector *axis = CheckVxVector(L, 2);
    const auto angle = static_cast<float>(luaL_checknumber(L, 3));
    entity->Rotate(axis, angle, OptCK3dEntity(L, 4), (OptBool(L, 5, false) ? TRUE : FALSE));
    return 0;
}

static int CK3dEntityLookAt(lua_State *L) {
    CheckMovableEntity(L, 1, "look_at")->LookAt(CheckVxVector(L, 2), OptCK3dEntity(L, 3), (OptBool(L, 4, false) ? TRUE : FALSE));
    return 0;
}

// entity:get_orientation([ref]) -> dir, up, right
static int CK3dEntityGetOrientation(lua_State *L) {
    VxVector dir, up, right;
    CheckCK3dEntity(L, 1)->GetOrientation(&dir, &up, &right, OptCK3dEntity(L, 2));
    PushVxVector(L, dir);
    PushVxVector(L, up);
    PushVxVector(L, right);
    return 3;
}

// entity:set_orientation(dir, up[, right[, ref[, keep_children]]])
static int CK3dEntitySetOrientation(lua_State *L) {
    auto *entity = CheckMovableEntity(L, 1, "set_orientation");
    const VxVector *dir = CheckVxVector(L, 2);
    const VxVector *up = CheckVxVector(L, 3);
    const VxVector *right = lua_isnoneornil(L, 4) ? nullptr : CheckVxVector(L, 4);
    entity->SetOrientation(dir, up, right, OptCK3dEntity(L, 5), (OptBool(L, 6, false) ? TRUE : FALSE));
    return 0;
}

static int CK3dEntityGetQuaternion(lua_State *L) {
    VxQuaternion quat;
    CheckCK3dEntity(L, 1)->GetQuaternion(&quat, OptCK3dEntity(L, 2));
    PushVxQuaternion(L, quat);
    return 1;
}

// entity:set_quaternion(quat[, ref[, keep_children[, keep_scale]]])
static int CK3dEntitySetQuaternion(lua_State *L) {
    auto *entity = CheckMovableEntity(L, 1, "set_quaternion");
    const auto *quat = CheckVxQuaternion(L, 2);
    entity->SetQuaternion(quat, OptCK3dEntity(L, 3), (OptBool(L, 4, false) ? TRUE : FALSE), (OptBool(L, 5, false) ? TRUE : FALSE));
    return 0;
}

// entity:set_scale(scale[, keep_children[, local]])
static int CK3dEntitySetScale(lua_State *L) {
    CheckMovableEntity(L, 1, "set_scale")->SetScale(CheckVxVector(L, 2), (OptBool(L, 3, false) ? TRUE : FALSE), (OptBool(L, 4, true) ? TRUE : FALSE));
    return 0;
}

static int CK3dEntityAddScale(lua_State *L) {
    CheckMovableEntity(L, 1, "add_scale")->AddScale(CheckVxVector(L, 2), (OptBool(L, 3, false) ? TRUE : FALSE), (OptBool(L, 4, true) ? TRUE : FALSE));
    return 0;
}

static int CK3dEntityGetWorldMatrix(lua_State *L) {
    PushVxMatrix(L, CheckCK3dEntity(L, 1)->GetWorldMatrix());
    return 1;
}

static int CK3dEntityGetLocalMatrix(lua_State *L) {
    PushVxMatrix(L, CheckCK3dEntity(L, 1)->GetLocalMatrix());
    return 1;
}

static int CK3dEntityGetInverseWorldMatrix(lua_State *L) {
    PushVxMatrix(L, CheckCK3dEntity(L, 1)->GetInverseWorldMatrix());
    return 1;
}

static int CK3dEntitySetWorldMatrix(lua_State *L) {
    auto *entity = CheckMovableEntity(L, 1, "set_world_matrix");
    entity->SetWorldMatrix(*CheckVxMatrix(L, 2), (OptBool(L, 3, false) ? TRUE : FALSE));
    return 0;
}

static int CK3dEntitySetLocalMatrix(lua_State *L) {
    auto *entity = CheckMovableEntity(L, 1, "set_local_matrix");
    entity->SetLocalMatrix(*CheckVxMatrix(L, 2), (OptBool(L, 3, false) ? TRUE : FALSE));
    return 0;
}

// entity:transform(local_point[, ref]) -> point in ref (world when ref is nil)
static int CK3dEntityTransform(lua_State *L) {
    VxVector result;
    CheckCK3dEntity(L, 1)->Transform(&result, CheckVxVector(L, 2), OptCK3dEntity(L, 3));
    PushVxVector(L, result);
    return 1;
}

static int CK3dEntityInverseTransform(lua_State *L) {
    VxVector result;
    CheckCK3dEntity(L, 1)->InverseTransform(&result, CheckVxVector(L, 2), OptCK3dEntity(L, 3));
    PushVxVector(L, result);
    return 1;
}

static int CK3dEntityGetBoundingBox(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    PushVxBbox(L, entity->GetBoundingBox((OptBool(L, 2, false) ? TRUE : FALSE)));
    return 1;
}

static int CK3dEntityGetHierarchicalBox(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    PushVxBbox(L, entity->GetHierarchicalBox((OptBool(L, 2, false) ? TRUE : FALSE)));
    return 1;
}

static int CK3dEntityGetBarycenter(lua_State *L) {
    VxVector center;
    if (!CheckCK3dEntity(L, 1)->GetBaryCenter(&center)) {
        lua_pushnil(L);
        return 1;
    }
    PushVxVector(L, center);
    return 1;
}

// entity:ray_intersection(from, to[, ref]) -> VxIntersectionDesc or nil.
// The ray is expressed in ref's frame; nil ref means world coordinates.
static int CK3dEntityRayIntersection(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    const VxVector *from = CheckVxVector(L, 2);
    const VxVector *to = CheckVxVector(L, 3);
    VxIntersectionDesc desc;
    if (!entity->RayIntersection(from, to, &desc, OptCK3dEntity(L, 4))) {
        lua_pushnil(L);
        return 1;
    }
    tas::lua::PushOwnedUserdata<VxIntersectionDesc>(L, kVxIntersectionDescMt, desc);
    return 1;
}

// entity:set_parent(parent_or_nil[, keep_world_pos])
static int CK3dEntitySetParent(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    lua_pushboolean(L, entity->SetParent(OptCK3dEntity(L, 2), (OptBool(L, 3, true) ? TRUE : FALSE)));
    return 1;
}

static int CK3dEntityAddChild(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    lua_pushboolean(L, entity->AddChild(CheckCK3dEntity(L, 2), (OptBool(L, 3, true) ? TRUE : FALSE)));
    return 1;
}

static int CK3dEntityRemoveChild(lua_State *L) {
    lua_pushboolean(L, CheckCK3dEntity(L, 1)->RemoveChild(CheckCK3dEntity(L, 2)));
    return 1;
}

static int CK3dEntityIsPickable(lua_State *L) {
    lua_pushboolean(L, CheckCK3dEntity(L, 1)->IsPickable());
    return 1;
}

static int CK3dEntitySetPickable(lua_State *L) {
    CheckCK3dEntity(L, 1)->SetPickable(lua_toboolean(L, 2) ? TRUE : FALSE);
    return 0;
}

static int CK3dEntityToString(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    char buffer[176];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "CK3dEntity(id=%d, class_id=%d, name=%s)",
                  entity ? entity->GetID() : 0,
                  entity ? entity->GetClassID() : 0,
                  entity && entity->GetName() ? entity->GetName() : "");
    lua_pushstring(L, buffer);
    return 1;
}

void LuaApi::RegisterCK3dEntity(lua_State *state) {
    tas::lua::LuaStackGuard guard(state);

    tas::lua::LuaUserdataRegistry<CK3dEntity>(state, kCK3dEntityMt)
        .Base<CKRenderObject>(kCKRenderObjectMt)
        .ReadonlyProperty("children_count", CK3dEntityChildrenCount)
        .ReadonlyProperty("flags", CK3dEntityFlags)
        .ReadonlyProperty("radius", CK3dEntityRadius)
        .ReadonlyProperty("parent", CK3dEntityParent)
        .Property("pickable", CK3dEntityIsPickable, CK3dEntitySetPickable)
        .Method("get_child", CK3dEntityGetChild)
        .Method("set_parent", CK3dEntitySetParent)
        .Method("add_child", CK3dEntityAddChild)
        .Method("remove_child", CK3dEntityRemoveChild)
        .Method("get_position", CK3dEntityGetPositionIn)
        .Method("set_position", CK3dEntitySetPosition)
        .Method("translate", CK3dEntityTranslate)
        .Method("rotate", CK3dEntityRotate)
        .Method("look_at", CK3dEntityLookAt)
        .Method("get_orientation", CK3dEntityGetOrientation)
        .Method("set_orientation", CK3dEntitySetOrientation)
        .Method("get_quaternion", CK3dEntityGetQuaternion)
        .Method("set_quaternion", CK3dEntitySetQuaternion)
        .Method("get_scale", CK3dEntityGetScale)
        .Method("set_scale", CK3dEntitySetScale)
        .Method("add_scale", CK3dEntityAddScale)
        .Method("get_world_matrix", CK3dEntityGetWorldMatrix)
        .Method("set_world_matrix", CK3dEntitySetWorldMatrix)
        .Method("get_local_matrix", CK3dEntityGetLocalMatrix)
        .Method("set_local_matrix", CK3dEntitySetLocalMatrix)
        .Method("get_inverse_world_matrix", CK3dEntityGetInverseWorldMatrix)
        .Method("transform", CK3dEntityTransform)
        .Method("inverse_transform", CK3dEntityInverseTransform)
        .Method("get_bounding_box", CK3dEntityGetBoundingBox)
        .Method("get_hierarchical_box", CK3dEntityGetHierarchicalBox)
        .Method("get_barycenter", CK3dEntityGetBarycenter)
        .Method("ray_intersection", CK3dEntityRayIntersection)
        .MetaMethod("__tostring", CK3dEntityToString);
}
