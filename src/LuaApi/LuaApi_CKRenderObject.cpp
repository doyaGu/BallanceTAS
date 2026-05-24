#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include <CKRenderObject.h>
#include <CKSceneObject.h>

namespace {

constexpr const char *kCKRenderObjectMt = "BallanceTAS.CKRenderObject";

CKRenderObject *CheckCKRenderObject(lua_State *L, int index) {
    return tas::lua::CheckUserdata<CKRenderObject>(L, index, kCKRenderObjectMt);
}

int IsRootObject(lua_State *L) {
    auto *object = CheckCKRenderObject(L, 1);
    lua_pushboolean(L, object && object->IsRootObject());
    return 1;
}

int IsToBeRendered(lua_State *L) {
    auto *object = CheckCKRenderObject(L, 1);
    lua_pushboolean(L, object && object->IsToBeRendered());
    return 1;
}

int IsToBeRenderedLast(lua_State *L) {
    auto *object = CheckCKRenderObject(L, 1);
    lua_pushboolean(L, object && object->IsToBeRenderedLast());
    return 1;
}

int ZOrder(lua_State *L) {
    auto *object = CheckCKRenderObject(L, 1);
    lua_pushinteger(L, object ? object->GetZOrder() : 0);
    return 1;
}

int SetZOrder(lua_State *L) {
    auto *object = CheckCKRenderObject(L, 1);
    if (!object) {
        return luaL_error(L, "CKRenderObject is null");
    }
    object->SetZOrder(static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

int ToString(lua_State *L) {
    auto *object = CheckCKRenderObject(L, 1);
    lua_pushfstring(L, "CKRenderObject(id=%d, name=%s)",
                    object ? object->GetID() : 0,
                    object && object->GetName() ? object->GetName() : "");
    return 1;
}

} // namespace

void LuaApi::RegisterCKRenderObject(lua_State *state) {
    tas::lua::LuaStackGuard guard(state);

    tas::lua::LuaUserdataRegistry<CKRenderObject>(state, kCKRenderObjectMt)
        .Base<CKSceneObject>("BallanceTAS.CKSceneObject")
        .ReadonlyProperty("is_root_object", IsRootObject)
        .ReadonlyProperty("is_to_be_rendered", IsToBeRendered)
        .ReadonlyProperty("is_to_be_rendered_last", IsToBeRenderedLast)
        .Property("z_order", ZOrder, SetZOrder)
        .MetaMethod("__tostring", ToString);
}
