#pragma once

#include "LuaRuntime/LuaHeaders.h"

#include <new>
#include <string>
#include <typeinfo>
#include <utility>

namespace tas::lua {

template <typename T>
class LuaYieldState {
public:
    template <typename... Args>
    static lua_KContext Create(lua_State *state, Args &&...args) {
        EnsureMetatable(state);

        auto *storage = static_cast<Storage *>(lua_newuserdatauv(state, sizeof(Storage), 0));
        storage->constructed = false;
        new (storage->Object()) T(std::forward<Args>(args)...);
        storage->constructed = true;

        luaL_getmetatable(state, MetatableName());
        lua_setmetatable(state, -2);

        const int ref = luaL_ref(state, LUA_REGISTRYINDEX);
        return static_cast<lua_KContext>(ref);
    }

    static T *Get(lua_State *state, lua_KContext ctx) {
        if (auto *value = TryGet(state, ctx)) {
            return value;
        }
        luaL_error(state, "invalid Lua yield continuation state");
        return nullptr;
    }

    static T *TryGet(lua_State *state, lua_KContext ctx) {
        lua_rawgeti(state, LUA_REGISTRYINDEX, ToRef(ctx));
        auto *storage = static_cast<Storage *>(luaL_testudata(state, -1, MetatableName()));
        T *value = storage && storage->constructed ? storage->Object() : nullptr;
        lua_pop(state, 1);
        return value;
    }

    static void Release(lua_State *state, lua_KContext ctx) {
        const int ref = ToRef(ctx);
        lua_rawgeti(state, LUA_REGISTRYINDEX, ref);
        auto *storage = static_cast<Storage *>(luaL_testudata(state, -1, MetatableName()));
        if (storage) {
            storage->Destroy();
        }
        lua_pop(state, 1);
        luaL_unref(state, LUA_REGISTRYINDEX, ref);
    }

private:
    struct Storage {
        bool constructed = false;
        alignas(T) unsigned char bytes[sizeof(T)];

        T *Object() {
            return reinterpret_cast<T *>(bytes);
        }

        void Destroy() {
            if (constructed) {
                Object()->~T();
                constructed = false;
            }
        }
    };

    static int Gc(lua_State *state) {
        auto *storage = static_cast<Storage *>(lua_touserdata(state, 1));
        if (storage) {
            storage->Destroy();
        }
        return 0;
    }

    static void EnsureMetatable(lua_State *state) {
        if (luaL_newmetatable(state, MetatableName())) {
            lua_pushcfunction(state, Gc);
            lua_setfield(state, -2, "__gc");
        }
        lua_pop(state, 1);
    }

    static const char *MetatableName() {
        static const std::string name = std::string("BallanceTAS.LuaYieldState.") + typeid(T).name();
        return name.c_str();
    }

    static int ToRef(lua_KContext ctx) {
        return static_cast<int>(ctx);
    }
};

} // namespace tas::lua
