#pragma once

#include <functional>

#include "LuaRuntime/LuaHeaders.h"

namespace tas::lua {

class LuaStackGuard {
public:
    explicit LuaStackGuard(lua_State *state)
        : m_State(state), m_ExpectedTop(state ? lua_gettop(state) : 0) {}

    LuaStackGuard(lua_State *state, int expectedTop)
        : m_State(state), m_ExpectedTop(expectedTop) {}

    LuaStackGuard(const LuaStackGuard &) = delete;
    LuaStackGuard &operator=(const LuaStackGuard &) = delete;

    ~LuaStackGuard() {
#ifndef NDEBUG
        IsBalanced();
#endif
    }

    bool IsBalanced(int delta = 0) const {
        if (!m_State) {
            return true;
        }
        return lua_gettop(m_State) == m_ExpectedTop + delta;
    }

    int ExpectedTop() const { return m_ExpectedTop; }
    int CurrentTop() const { return m_State ? lua_gettop(m_State) : 0; }

private:
    lua_State *m_State = nullptr;
    int m_ExpectedTop = 0;
};

} // namespace tas::lua
