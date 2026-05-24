#pragma once

#include "LuaRuntime/LuaHeaders.h"
#include "Result.h"

namespace tas::lua {

Result<void> ProtectedCall(lua_State *state, int argCount, int resultCount);

} // namespace tas::lua
