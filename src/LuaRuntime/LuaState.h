#pragma once

#include <string>

#include "LuaRuntime/LuaHeaders.h"
#include "Result.h"

namespace tas::lua {

class LuaState {
public:
    LuaState();
    ~LuaState();

    LuaState(const LuaState &) = delete;
    LuaState &operator=(const LuaState &) = delete;

    LuaState(LuaState &&other) noexcept;
    LuaState &operator=(LuaState &&other) noexcept;

    lua_State *Get() const { return m_State; }
    lua_State *MainThread() const;

    void OpenStandardLibraries();
    Result<void> LoadString(const std::string &script, const std::string &chunkName = "chunk");
    Result<void> LoadFile(const std::string &path);

    static int Traceback(lua_State *state);

private:
    static int Panic(lua_State *state);

    lua_State *m_State = nullptr;
};

} // namespace tas::lua
