#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include <CK3dEntity.h>
#include <CKRenderObject.h>
#include <cstdio>
#include <VxMath.h>

constexpr const char *kCK3dEntityMt = "BallanceTAS.CK3dEntity";

static CK3dEntity *CheckCK3dEntity(lua_State *L, int index) {
    return tas::lua::CheckUserdata<CK3dEntity>(L, index, kCK3dEntityMt);
}

static void PushCK3dEntity(lua_State *L, CK3dEntity *entity) {
    if (!entity) {
        lua_pushnil(L);
        return;
    }
    tas::lua::PushBorrowedUserdata<CK3dEntity>(L, kCK3dEntityMt, entity);
}

static void PushVxVectorValue(lua_State *L, const VxVector &value) {
    lua_getglobal(L, "VxVector");
    lua_pushnumber(L, value.x);
    lua_pushnumber(L, value.y);
    lua_pushnumber(L, value.z);
    lua_call(L, 3, 1);
}

static int CK3dEntityName(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    lua_pushstring(L, entity && entity->GetName() ? entity->GetName() : "");
    return 1;
}

static int CK3dEntitySetName(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    const char *name = luaL_checkstring(L, 2);
    entity->SetName(const_cast<char *>(name));
    return 0;
}

static int CK3dEntityId(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    lua_pushinteger(L, entity ? entity->GetID() : 0);
    return 1;
}

static int CK3dEntityClassId(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    lua_pushinteger(L, entity ? entity->GetClassID() : 0);
    return 1;
}

static int CK3dEntityObjectFlags(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    lua_pushinteger(L, entity ? entity->GetObjectFlags() : 0);
    return 1;
}

static int CK3dEntityIsVisible(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    lua_pushboolean(L, entity && entity->IsVisible());
    return 1;
}

static int CK3dEntityChildrenCount(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    lua_pushinteger(L, entity ? entity->GetChildrenCount() : 0);
    return 1;
}

static int CK3dEntityFlags(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    lua_pushinteger(L, entity ? entity->GetFlags() : 0);
    return 1;
}

static int CK3dEntityRadius(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    lua_pushnumber(L, entity ? entity->GetRadius() : 0.0f);
    return 1;
}

static int CK3dEntityParent(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    PushCK3dEntity(L, entity ? entity->GetParent() : nullptr);
    return 1;
}

static int CK3dEntityGetChild(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    const int index = static_cast<int>(luaL_checkinteger(L, 2));
    PushCK3dEntity(L, entity && index >= 0 ? entity->GetChild(index) : nullptr);
    return 1;
}

static int CK3dEntityGetPosition(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    VxVector position;
    if (entity) {
        entity->GetPosition(&position);
    } else {
        position.x = position.y = position.z = 0.0f;
    }
    PushVxVectorValue(L, position);
    return 1;
}

static int CK3dEntityGetScale(lua_State *L) {
    auto *entity = CheckCK3dEntity(L, 1);
    VxVector scale;
    if (entity) {
        entity->GetScale(&scale);
    } else {
        scale.x = scale.y = scale.z = 0.0f;
    }
    PushVxVectorValue(L, scale);
    return 1;
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
        .Base<CKRenderObject>("BallanceTAS.CKRenderObject")
        .Property("name", CK3dEntityName, CK3dEntitySetName)
        .ReadonlyProperty("id", CK3dEntityId)
        .ReadonlyProperty("class_id", CK3dEntityClassId)
        .ReadonlyProperty("object_flags", CK3dEntityObjectFlags)
        .ReadonlyProperty("is_visible", CK3dEntityIsVisible)
        .ReadonlyProperty("children_count", CK3dEntityChildrenCount)
        .ReadonlyProperty("flags", CK3dEntityFlags)
        .ReadonlyProperty("radius", CK3dEntityRadius)
        .ReadonlyProperty("parent", CK3dEntityParent)
        .Method("get_child", CK3dEntityGetChild)
        .Method("get_position", CK3dEntityGetPosition)
        .Method("get_scale", CK3dEntityGetScale)
        .MetaMethod("__tostring", CK3dEntityToString);
}
