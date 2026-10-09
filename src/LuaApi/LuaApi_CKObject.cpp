#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include <CKObject.h>
#include <cstdio>

constexpr const char *kCKObjectMt = "BallanceTAS.CKObject";

static CKObject *CheckCKObject(lua_State *L, int index) {
    return tas::lua::CheckUserdata<CKObject>(L, index, kCKObjectMt);
}

static int CKObjectName(lua_State *L) {
    auto *object = CheckCKObject(L, 1);
    lua_pushstring(L, object && object->GetName() ? object->GetName() : "");
    return 1;
}

static int CKObjectSetName(lua_State *L) {
    auto *object = CheckCKObject(L, 1);
    const char *name = luaL_checkstring(L, 2);
    if (!object) {
        return luaL_error(L, "CKObject is null");
    }
    object->SetName(const_cast<char *>(name));
    return 0;
}

static int CKObjectId(lua_State *L) {
    auto *object = CheckCKObject(L, 1);
    lua_pushinteger(L, object ? object->GetID() : 0);
    return 1;
}

static int CKObjectClassId(lua_State *L) {
    auto *object = CheckCKObject(L, 1);
    lua_pushinteger(L, object ? object->GetClassID() : 0);
    return 1;
}

static int CKObjectFlags(lua_State *L) {
    auto *object = CheckCKObject(L, 1);
    lua_pushinteger(L, object ? object->GetObjectFlags() : 0);
    return 1;
}

static int CKObjectIsDynamic(lua_State *L) {
    auto *object = CheckCKObject(L, 1);
    lua_pushboolean(L, object && object->IsDynamic());
    return 1;
}

static int CKObjectIsVisible(lua_State *L) {
    auto *object = CheckCKObject(L, 1);
    lua_pushboolean(L, object && object->IsVisible());
    return 1;
}

// object:show([visible_or_option]): true/CKSHOW, false/CKHIDE or CKHIERARCHICALHIDE.
static int CKObjectShow(lua_State *L) {
    auto *object = CheckCKObject(L, 1);
    CK_OBJECT_SHOWOPTION option = CKSHOW;
    if (lua_isboolean(L, 2)) {
        option = lua_toboolean(L, 2) ? CKSHOW : CKHIDE;
    } else if (!lua_isnoneornil(L, 2)) {
        const lua_Integer value = luaL_checkinteger(L, 2);
        if (value != CKHIDE && value != CKSHOW && value != CKHIERARCHICALHIDE) {
            return luaL_error(L, "CKObject:show: expected a boolean or CKSHOW/CKHIDE/CKHIERARCHICALHIDE");
        }
        option = static_cast<CK_OBJECT_SHOWOPTION>(value);
    }
    object->Show(option);
    return 0;
}

static int CKObjectIsHiddenByParent(lua_State *L) {
    lua_pushboolean(L, CheckCKObject(L, 1)->IsHiddenByParent());
    return 1;
}

static int CKObjectIsHierarchicallyHidden(lua_State *L) {
    lua_pushboolean(L, CheckCKObject(L, 1)->IsHierarchicallyHide());
    return 1;
}

static int CKObjectCanBeHidden(lua_State *L) {
    lua_pushboolean(L, CheckCKObject(L, 1)->CanBeHide() != 0);
    return 1;
}

static int CKObjectIsToBeDeleted(lua_State *L) {
    lua_pushboolean(L, CheckCKObject(L, 1)->IsToBeDeleted() != 0);
    return 1;
}

static int CKObjectIsUpToDate(lua_State *L) {
    lua_pushboolean(L, CheckCKObject(L, 1)->IsUpToDate() != 0);
    return 1;
}

static int CKObjectModifyObjectFlags(lua_State *L) {
    auto *object = CheckCKObject(L, 1);
    const auto add = static_cast<CKDWORD>(luaL_checkinteger(L, 2));
    const auto remove = static_cast<CKDWORD>(luaL_optinteger(L, 3, 0));
    object->ModifyObjectFlags(add, remove);
    return 0;
}

static int CKObjectToString(lua_State *L) {
    auto *object = CheckCKObject(L, 1);
    char buffer[160];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "CKObject(id=%d, class_id=%d, name=%s)",
                  object ? object->GetID() : 0,
                  object ? object->GetClassID() : 0,
                  object && object->GetName() ? object->GetName() : "");
    lua_pushstring(L, buffer);
    return 1;
}

void LuaApi::RegisterCKObject(lua_State *state) {
    tas::lua::LuaStackGuard guard(state);

    tas::lua::LuaUserdataRegistry<CKObject>(state, kCKObjectMt)
        .Property("name", CKObjectName, CKObjectSetName)
        .ReadonlyProperty("id", CKObjectId)
        .ReadonlyProperty("class_id", CKObjectClassId)
        .ReadonlyProperty("object_flags", CKObjectFlags)
        .ReadonlyProperty("is_dynamic", CKObjectIsDynamic)
        .ReadonlyProperty("is_visible", CKObjectIsVisible)
        .ReadonlyProperty("is_hidden_by_parent", CKObjectIsHiddenByParent)
        .ReadonlyProperty("is_hierarchically_hidden", CKObjectIsHierarchicallyHidden)
        .ReadonlyProperty("can_be_hidden", CKObjectCanBeHidden)
        .ReadonlyProperty("is_to_be_deleted", CKObjectIsToBeDeleted)
        .ReadonlyProperty("is_up_to_date", CKObjectIsUpToDate)
        .Method("show", CKObjectShow)
        .Method("modify_object_flags", CKObjectModifyObjectFlags)
        .MetaMethod("__tostring", CKObjectToString);
}
