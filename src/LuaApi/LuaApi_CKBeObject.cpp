#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include <CKBeObject.h>
#include <CKObject.h>

namespace {

constexpr const char *kCKBeObjectMt = "BallanceTAS.CKBeObject";

CKBeObject *CheckCKBeObject(lua_State *L, int index) {
    return tas::lua::CheckUserdata<CKBeObject>(L, index, kCKBeObjectMt);
}

int Priority(lua_State *L) {
    auto *object = CheckCKBeObject(L, 1);
    lua_pushinteger(L, object ? object->GetPriority() : 0);
    return 1;
}

int SetPriority(lua_State *L) {
    auto *object = CheckCKBeObject(L, 1);
    if (!object) {
        return luaL_error(L, "CKBeObject is null");
    }
    object->SetPriority(static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

int LastExecutionTime(lua_State *L) {
    auto *object = CheckCKBeObject(L, 1);
    lua_pushnumber(L, object ? object->GetLastExecutionTime() : 0.0);
    return 1;
}

int ToString(lua_State *L) {
    auto *object = CheckCKBeObject(L, 1);
    lua_pushfstring(L, "CKBeObject(id=%d, name=%s)",
                    object ? object->GetID() : 0,
                    object && object->GetName() ? object->GetName() : "");
    return 1;
}

} // namespace

void LuaApi::RegisterCKBeObject(lua_State *state) {
    tas::lua::LuaStackGuard guard(state);

    tas::lua::LuaUserdataRegistry<CKBeObject>(state, kCKBeObjectMt)
        .Base<CKObject>("BallanceTAS.CKObject")
        .Property("priority", Priority, SetPriority)
        .ReadonlyProperty("last_execution_time", LastExecutionTime)
        .MetaMethod("__tostring", ToString);
}
