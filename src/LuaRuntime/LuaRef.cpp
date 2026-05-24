#include "LuaRuntime/LuaRef.h"

#include <utility>

namespace tas::lua {

LuaRef::~LuaRef() {
    Reset();
}

LuaRef::LuaRef(LuaRef &&other) noexcept
    : m_State(std::exchange(other.m_State, nullptr)),
      m_Ref(std::exchange(other.m_Ref, LUA_NOREF)) {}

LuaRef &LuaRef::operator=(LuaRef &&other) noexcept {
    if (this == &other) {
        return *this;
    }
    Reset();
    m_State = std::exchange(other.m_State, nullptr);
    m_Ref = std::exchange(other.m_Ref, LUA_NOREF);
    return *this;
}

LuaRef LuaRef::FromStack(lua_State *state, int index) {
    if (!state) {
        return {};
    }
    lua_rawgeti(state, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
    lua_State *refState = lua_tothread(state, -1);
    lua_pop(state, 1);
    if (!refState) {
        refState = state;
    }

    const int absolute = lua_absindex(state, index);
    lua_pushvalue(state, absolute);
    const int ref = luaL_ref(state, LUA_REGISTRYINDEX);
    lua_remove(state, absolute);
    return LuaRef(refState, ref);
}

void LuaRef::Push() const {
    Push(m_State);
}

void LuaRef::Push(lua_State *targetState) const {
    if (!targetState) {
        return;
    }
    if (m_Ref == LUA_REFNIL) {
        lua_pushnil(targetState);
        return;
    }
    lua_rawgeti(targetState, LUA_REGISTRYINDEX, m_Ref);
}

LuaRef LuaRef::Clone() const {
    if (!IsValid()) {
        return {};
    }
    Push();
    return FromStack(m_State, -1);
}

void LuaRef::Reset() {
    if (m_State && m_Ref != LUA_NOREF && m_Ref != LUA_REFNIL) {
        luaL_unref(m_State, LUA_REGISTRYINDEX, m_Ref);
    }
    m_State = nullptr;
    m_Ref = LUA_NOREF;
}

} // namespace tas::lua
