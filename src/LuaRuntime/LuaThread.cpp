#include "LuaRuntime/LuaThread.h"

#include <string>

namespace tas::lua {

LuaThread LuaThread::CreateFromFunction(lua_State *mainState, int functionIndex) {
    if (!mainState || !lua_isfunction(mainState, functionIndex)) {
        return {};
    }

    const int absoluteFunction = lua_absindex(mainState, functionIndex);
    lua_State *threadState = lua_newthread(mainState);
    lua_pushvalue(mainState, absoluteFunction);
    lua_xmove(mainState, threadState, 1);

    LuaRef threadRef = LuaRef::FromStack(mainState, -1);
    return LuaThread(threadState, std::move(threadRef));
}

LuaThreadStatus LuaThread::Status() const {
    if (!m_State) {
        return LuaThreadStatus::Error;
    }
    if (m_Completed || m_HadError) {
        return LuaThreadStatus::Dead;
    }
    const int status = lua_status(m_State);
    if (status == LUA_YIELD) {
        return LuaThreadStatus::Yielded;
    }
    if (status == LUA_OK) {
        return LuaThreadStatus::Runnable;
    }
    return LuaThreadStatus::Error;
}

Result<int> LuaThread::Resume(int argCount, lua_State *from) {
    if (!IsValid()) {
        return Result<int>::Error("Lua thread is invalid", "lua.thread");
    }
    if (m_Completed) {
        return Result<int>::Error("Lua thread is already dead", "lua.thread");
    }
    if (argCount < 0 || lua_gettop(m_State) < argCount) {
        return Result<int>::Error("Lua thread resume argument count is invalid", "lua.thread");
    }

    int resultCount = 0;
    const int status = lua_resume(m_State, from, argCount, &resultCount);
    if (status == LUA_OK) {
        m_Completed = true;
        return Result<int>::Ok(resultCount);
    }
    if (status == LUA_YIELD) {
        return Result<int>::Ok(resultCount);
    }

    m_HadError = true;
    std::string traceback = BuildTraceback(m_State);
    lua_settop(m_State, 0);
    return Result<int>::Error(std::move(traceback), "lua.thread");
}

void LuaThread::Reset() {
    m_State = nullptr;
    m_ThreadRef.Reset();
    m_Completed = false;
    m_HadError = false;
}

std::string LuaThread::BuildTraceback(lua_State *threadState) {
    size_t messageLength = 0;
    const char *message = luaL_tolstring(threadState, -1, &messageLength);
    std::string messageText = message
        ? std::string(message, messageLength)
        : std::string("(non-string Lua error)");
    lua_pop(threadState, 1);
    if (messageText.empty()) {
        messageText = "(empty Lua error)";
    }

    luaL_traceback(threadState, threadState, messageText.c_str(), 1);
    const char *traceback = lua_tostring(threadState, -1);
    return traceback ? std::string(traceback) : std::string("Lua thread error");
}

} // namespace tas::lua
