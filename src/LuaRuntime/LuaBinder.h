#pragma once

#include <string>

#include "LuaRuntime/LuaHeaders.h"
#include "LuaRuntime/LuaRef.h"

namespace tas::lua {

class LuaBinder {
public:
    class Table {
    public:
        Table() = default;
        Table(lua_State *state, LuaRef ref) : m_State(state), m_Ref(std::move(ref)) {}

        Table(const Table &) = delete;
        Table &operator=(const Table &) = delete;
        Table(Table &&) noexcept = default;
        Table &operator=(Table &&) noexcept = default;

        Table CreateTable(const char *name) {
            Push();
            lua_newtable(m_State);
            lua_pushvalue(m_State, -1);
            LuaRef childRef = LuaRef::FromStack(m_State, -1);
            lua_setfield(m_State, -2, name);
            lua_pop(m_State, 1);
            return Table(m_State, std::move(childRef));
        }

        void SetFunction(const char *name, lua_CFunction function) {
            Push();
            lua_pushcfunction(m_State, function);
            lua_setfield(m_State, -2, name);
            lua_pop(m_State, 1);
        }

        void SetAlias(const char *name, const Table &source, const char *sourceName) {
            Push();
            source.Push();
            lua_getfield(m_State, -1, sourceName);
            lua_setfield(m_State, -3, name);
            lua_pop(m_State, 2);
        }

        void Push() const { m_Ref.Push(m_State); }
        lua_State *State() const { return m_State; }

    private:
        lua_State *m_State = nullptr;
        LuaRef m_Ref;
    };

    explicit LuaBinder(lua_State *state) : m_State(state) {}

    Table CreateGlobalTable(const char *name) {
        lua_newtable(m_State);
        lua_pushvalue(m_State, -1);
        LuaRef ref = LuaRef::FromStack(m_State, -1);
        lua_setglobal(m_State, name);
        return Table(m_State, std::move(ref));
    }

private:
    lua_State *m_State = nullptr;
};

} // namespace tas::lua
