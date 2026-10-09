#include "LuaApiCommon.h"

#include <cstdarg>
#include <cstring>
#include <stdexcept>


namespace LuaApiCommon {
    ScriptContext *GetRegistryContext(lua_State *L) {
        lua_getfield(L, LUA_REGISTRYINDEX, kContextRegistryKey);
        auto *context = static_cast<ScriptContext *>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        return context;
    }

    void PushTasSubtable(lua_State *L, const char *name) {
        lua_getglobal(L, "tas");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setglobal(L, "tas");
        }
        if (lua_getfield(L, -1, name) != LUA_TTABLE) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setfield(L, -3, name);
        }
        lua_remove(L, -2);
    }

    int Raise(lua_State *L, const char *where, const char *format, ...) {
        va_list args;
        va_start(args, format);
        luaL_where(L, 1); // "script.lua:12: " of the calling Lua code, like luaL_error
        lua_pushstring(L, where);
        lua_pushliteral(L, ": ");
        lua_pushvfstring(L, format, args);
        va_end(args);
        lua_concat(L, 4);
        return lua_error(L);
    }

    int RaiseArg(lua_State *L, const char *where, int arg, const char *expected) {
        return Raise(L, where, "bad argument #%d (%s)", arg, expected);
    }

    const char *CheckNonEmptyString(lua_State *L, int arg, const char *where, size_t *length) {
        size_t size = 0;
        const char *value = lua_type(L, arg) == LUA_TSTRING ? lua_tolstring(L, arg, &size) : nullptr;
        if (!value || size == 0) {
            RaiseArg(L, where, arg, "non-empty string expected");
        }
        if (length) {
            *length = size;
        }
        return value;
    }

    size_t CheckIndex(lua_State *L, int arg, const char *where) {
        int isInteger = 0;
        const lua_Integer value = lua_tointegerx(L, arg, &isInteger);
        if (!isInteger) {
            RaiseArg(L, where, arg, "integer expected");
        }
        if (value < 0) {
            RaiseArg(L, where, arg, "non-negative integer expected");
        }
        return static_cast<size_t>(value);
    }

    size_t OptIndex(lua_State *L, int arg, const char *where, size_t fallback) {
        return lua_isnoneornil(L, arg) ? fallback : CheckIndex(L, arg, where);
    }

    int CheckTicks(lua_State *L, int arg, const char *where) {
        if (!lua_isinteger(L, arg)) {
            RaiseArg(L, where, arg, "integer tick count expected");
        }
        const lua_Integer ticks = lua_tointeger(L, arg);
        if (ticks <= 0) {
            RaiseArg(L, where, arg, "tick count must be positive");
        }
        return static_cast<int>(ticks);
    }

    void PushStringList(lua_State *L, const std::vector<std::string> &values) {
        lua_createtable(L, static_cast<int>(values.size()), 0);
        for (size_t i = 0; i < values.size(); ++i) {
            PushString(L, values[i]);
            lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
        }
    }

    void PushIndexList(lua_State *L, const std::vector<size_t> &values) {
        lua_createtable(L, static_cast<int>(values.size()), 0);
        for (size_t i = 0; i < values.size(); ++i) {
            lua_pushinteger(L, static_cast<lua_Integer>(values[i]));
            lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
        }
    }

    void RunInstallChunk(lua_State *L, const char *chunk, const char *chunkName, int results) {
        if (luaL_loadbufferx(L, chunk, std::strlen(chunk), chunkName, "t") != LUA_OK ||
            lua_pcall(L, 0, results, 0) != LUA_OK) {
            std::string message = lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown error";
            lua_pop(L, 1);
            throw std::runtime_error(std::string(chunkName) + " failed: " + message);
        }
    }
}
