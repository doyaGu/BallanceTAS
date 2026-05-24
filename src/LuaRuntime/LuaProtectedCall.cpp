#include "LuaRuntime/LuaProtectedCall.h"

#include <string>

#include "LuaRuntime/LuaState.h"

namespace tas::lua {

Result<void> ProtectedCall(lua_State *state, int argCount, int resultCount) {
    if (!state) {
        return Result<void>::Error("Lua state is null", "lua.call");
    }

    const int functionIndex = lua_gettop(state) - argCount;
    lua_pushcfunction(state, &LuaState::Traceback);
    lua_insert(state, functionIndex);

    const int status = lua_pcall(state, argCount, resultCount, functionIndex);
    if (status == LUA_OK) {
        lua_remove(state, functionIndex);
        return Result<void>::Ok();
    }

    std::string message = lua_tostring(state, -1) ? lua_tostring(state, -1) : "Lua protected call failed";
    lua_pop(state, 1);
    lua_remove(state, functionIndex);
    return Result<void>::Error(std::move(message), "lua.call");
}

} // namespace tas::lua
