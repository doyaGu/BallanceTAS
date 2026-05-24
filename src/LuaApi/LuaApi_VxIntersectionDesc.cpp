#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include <CKRenderObject.h>
#include <CKTypes.h>
#include <string>

constexpr const char *kVxIntersectionDescMt = "BallanceTAS.VxIntersectionDesc";
constexpr const char *kCKRenderObjectMt = "BallanceTAS.CKRenderObject";

static VxIntersectionDesc *CheckDesc(lua_State *L, int index) {
    return tas::lua::CheckUserdata<VxIntersectionDesc>(L, index, kVxIntersectionDescMt);
}

static void PushVxVector(lua_State *L, const VxVector &value) {
    lua_getglobal(L, "VxVector");
    lua_pushnumber(L, value.x);
    lua_pushnumber(L, value.y);
    lua_pushnumber(L, value.z);
    lua_call(L, 3, 1);
}

static int Object(lua_State *L) {
    auto *desc = CheckDesc(L, 1);
    if (!desc || !desc->Object) {
        lua_pushnil(L);
        return 1;
    }
    tas::lua::PushBorrowedUserdata<CKRenderObject>(L, kCKRenderObjectMt, desc->Object);
    return 1;
}

static int Point(lua_State *L) {
    PushVxVector(L, CheckDesc(L, 1)->IntersectionPoint);
    return 1;
}

static int Normal(lua_State *L) {
    PushVxVector(L, CheckDesc(L, 1)->IntersectionNormal);
    return 1;
}

static int U(lua_State *L) {
    lua_pushnumber(L, CheckDesc(L, 1)->TexU);
    return 1;
}

static int V(lua_State *L) {
    lua_pushnumber(L, CheckDesc(L, 1)->TexV);
    return 1;
}

static int Distance(lua_State *L) {
    lua_pushnumber(L, CheckDesc(L, 1)->Distance);
    return 1;
}

static int FaceIndex(lua_State *L) {
    lua_pushinteger(L, CheckDesc(L, 1)->FaceIndex);
    return 1;
}

static int ToString(lua_State *L) {
    auto *desc = CheckDesc(L, 1);
    std::string objectName = "nil";
    if (desc && desc->Object) {
        objectName = desc->Object->GetName() ? desc->Object->GetName() : ("ID: " + std::to_string(desc->Object->GetID()));
    }
    const std::string text = "VxIntersectionDesc(obj: " + objectName +
                             ", dist: " + std::to_string(desc ? desc->Distance : 0.0f) +
                             ", face: " + std::to_string(desc ? desc->FaceIndex : 0) + ")";
    lua_pushlstring(L, text.data(), text.size());
    return 1;
}

void LuaApi::RegisterVxIntersectionDesc(lua_State *state) {
    tas::lua::LuaStackGuard guard(state);

    tas::lua::LuaUserdataRegistry<VxIntersectionDesc>(state, kVxIntersectionDescMt)
        .ReadonlyProperty("object", Object)
        .ReadonlyProperty("point", Point)
        .ReadonlyProperty("normal", Normal)
        .ReadonlyProperty("u", U)
        .ReadonlyProperty("v", V)
        .ReadonlyProperty("distance", Distance)
        .ReadonlyProperty("face_index", FaceIndex)
        .MetaMethod("__tostring", ToString);
}
