#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include <CKBeObject.h>
#include <CKSceneObject.h>

namespace {

constexpr const char *kCKSceneObjectMt = "BallanceTAS.CKSceneObject";

CKSceneObject *CheckCKSceneObject(lua_State *L, int index) {
    return tas::lua::CheckUserdata<CKSceneObject>(L, index, kCKSceneObjectMt);
}

int IsActiveInCurrentScene(lua_State *L) {
    auto *object = CheckCKSceneObject(L, 1);
    lua_pushboolean(L, object && object->IsActiveInCurrentScene());
    return 1;
}

int ToString(lua_State *L) {
    auto *object = CheckCKSceneObject(L, 1);
    lua_pushfstring(L, "CKSceneObject(id=%d, name=%s)",
                    object ? object->GetID() : 0,
                    object && object->GetName() ? object->GetName() : "");
    return 1;
}

} // namespace

void LuaApi::RegisterCKSceneObject(lua_State *state) {
    tas::lua::LuaStackGuard guard(state);

    tas::lua::LuaUserdataRegistry<CKSceneObject>(state, kCKSceneObjectMt)
        .Base<CKBeObject>("BallanceTAS.CKBeObject")
        .ReadonlyProperty("is_active_in_current_scene", IsActiveInCurrentScene)
        .MetaMethod("__tostring", ToString);
}
