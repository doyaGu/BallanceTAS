#include "LuaRuntime/LuaFunction.h"

#include "LuaRuntime/LuaProtectedCall.h"

namespace tas::lua {

LuaFunction LuaFunction::FromStack(lua_State *state, int index) {
    if (!state || !lua_isfunction(state, index)) {
        return {};
    }
    return LuaFunction(LuaRef::FromStack(state, index));
}

Result<int> LuaFunction::Call(int argCount, int resultCount, const ArgumentPusher &pushArguments) const {
    lua_State *state = State();
    if (!IsValid() || !state) {
        return Result<int>::Error("Lua function is invalid", "lua.function");
    }
    if (argCount < 0 || resultCount < 0) {
        return Result<int>::Error("Lua function call counts cannot be negative", "lua.function");
    }

    Push();
    if (pushArguments) {
        pushArguments(state);
    }

    auto call = ProtectedCall(state, argCount, resultCount);
    if (call.IsError()) {
        return Result<int>::Error(call.GetError());
    }
    return Result<int>::Ok(resultCount);
}

} // namespace tas::lua
