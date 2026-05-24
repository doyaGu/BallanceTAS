#pragma once

#include "LuaRuntime/LuaHeaders.h"

namespace tas::lua {

class LuaRef {
public:
    LuaRef() = default;
    ~LuaRef();

    LuaRef(const LuaRef &) = delete;
    LuaRef &operator=(const LuaRef &) = delete;

    LuaRef(LuaRef &&other) noexcept;
    LuaRef &operator=(LuaRef &&other) noexcept;

    static LuaRef FromStack(lua_State *state, int index);

    bool IsValid() const { return m_State && m_Ref != LUA_NOREF && m_Ref != LUA_REFNIL; }
    lua_State *State() const { return m_State; }
    int RawRef() const { return m_Ref; }

    void Push() const;
    void Push(lua_State *targetState) const;
    LuaRef Clone() const;
    void Reset();

private:
    LuaRef(lua_State *state, int ref) : m_State(state), m_Ref(ref) {}

    lua_State *m_State = nullptr;
    int m_Ref = LUA_NOREF;
};

} // namespace tas::lua
