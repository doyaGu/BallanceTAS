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
        .MetaMethod("__tostring", CKObjectToString);
}
