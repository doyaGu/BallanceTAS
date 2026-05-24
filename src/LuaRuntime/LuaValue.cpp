#include "LuaRuntime/LuaValue.h"

namespace tas::lua {

bool LuaValue::IsTable() const {
    return std::holds_alternative<TablePtr>(m_Value);
}

const LuaValue *LuaValue::FindField(const std::string &key) const {
    const auto *table = std::get_if<TablePtr>(&m_Value);
    if (!table || !*table) {
        return nullptr;
    }
    for (const auto &entry : (*table)->entries) {
        const auto *stringKey = std::get_if<std::string>(&entry.key.value);
        if (stringKey && *stringKey == key) {
            return entry.value.get();
        }
    }
    return nullptr;
}

std::string LuaValue::GetStringField(const std::string &key, std::string defaultValue) const {
    const LuaValue *value = FindField(key);
    if (!value) {
        return defaultValue;
    }
    const auto *text = std::get_if<std::string>(&value->m_Value);
    return text ? *text : std::move(defaultValue);
}

lua_Number LuaValue::GetNumberField(const std::string &key, lua_Number defaultValue) const {
    const LuaValue *value = FindField(key);
    if (!value) {
        return defaultValue;
    }
    if (const auto *integer = std::get_if<lua_Integer>(&value->m_Value)) {
        return static_cast<lua_Number>(*integer);
    }
    if (const auto *number = std::get_if<lua_Number>(&value->m_Value)) {
        return *number;
    }
    return defaultValue;
}

lua_Integer LuaValue::GetIntegerField(const std::string &key, lua_Integer defaultValue) const {
    const LuaValue *value = FindField(key);
    if (!value) {
        return defaultValue;
    }
    if (const auto *integer = std::get_if<lua_Integer>(&value->m_Value)) {
        return *integer;
    }
    return defaultValue;
}

bool LuaValue::GetBoolField(const std::string &key, bool defaultValue) const {
    const LuaValue *value = FindField(key);
    if (!value) {
        return defaultValue;
    }
    const auto *boolean = std::get_if<bool>(&value->m_Value);
    return boolean ? *boolean : defaultValue;
}

Result<LuaValue> LuaValue::FromStack(lua_State *state, int index, int maxDepth) {
    if (!state) {
        return Result<LuaValue>::Error("Lua state is null", "lua.value");
    }
    return FromStackImpl(state, lua_absindex(state, index), maxDepth);
}

Result<LuaValue> LuaValue::FromStackImpl(lua_State *state, int index, int remainingDepth) {
    switch (lua_type(state, index)) {
    case LUA_TNIL:
        return Result<LuaValue>::Ok(LuaValue{std::monostate{}});
    case LUA_TBOOLEAN:
        return Result<LuaValue>::Ok(LuaValue{static_cast<bool>(lua_toboolean(state, index))});
    case LUA_TNUMBER:
        if (lua_isinteger(state, index)) {
            return Result<LuaValue>::Ok(LuaValue{lua_tointeger(state, index)});
        }
        return Result<LuaValue>::Ok(LuaValue{lua_tonumber(state, index)});
    case LUA_TSTRING: {
        size_t len = 0;
        const char *text = lua_tolstring(state, index, &len);
        return Result<LuaValue>::Ok(LuaValue{std::string(text, len)});
    }
    case LUA_TTABLE: {
        if (remainingDepth <= 0) {
            return Result<LuaValue>::Error("Lua table exceeds maximum serialization depth", "lua.value");
        }
        auto table = std::make_shared<Table>();
        lua_pushnil(state);
        while (lua_next(state, index) != 0) {
            const int keyType = lua_type(state, -2);
            Key key;
            if (keyType == LUA_TSTRING) {
                size_t len = 0;
                const char *text = lua_tolstring(state, -2, &len);
                key.value = std::string(text, len);
            } else if (keyType == LUA_TNUMBER && lua_isinteger(state, -2)) {
                key.value = lua_tointeger(state, -2);
            } else {
                lua_pop(state, 1);
                return Result<LuaValue>::Error("Lua table contains unsupported key type", "lua.value");
            }
            auto child = FromStackImpl(state, lua_absindex(state, -1), remainingDepth - 1);
            if (child.IsError()) {
                lua_pop(state, 1);
                return child;
            }
            table->entries.push_back(Entry{
                std::move(key),
                std::make_shared<LuaValue>(std::move(child.Unwrap()))
            });
            lua_pop(state, 1);
        }
        return Result<LuaValue>::Ok(LuaValue{table});
    }
    default:
        return Result<LuaValue>::Error("Lua value type is not portable across contexts", "lua.value");
    }
}

void LuaValue::Push(lua_State *state) const {
    std::visit([state](const auto &value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            lua_pushnil(state);
        } else if constexpr (std::is_same_v<T, bool>) {
            lua_pushboolean(state, value ? 1 : 0);
        } else if constexpr (std::is_same_v<T, lua_Integer>) {
            lua_pushinteger(state, value);
        } else if constexpr (std::is_same_v<T, lua_Number>) {
            lua_pushnumber(state, value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            lua_pushlstring(state, value.data(), value.size());
        } else if constexpr (std::is_same_v<T, TablePtr>) {
            lua_newtable(state);
            if (!value) {
                return;
            }
            for (const auto &entry : value->entries) {
                std::visit([state](const auto &key) {
                    using K = std::decay_t<decltype(key)>;
                    if constexpr (std::is_same_v<K, lua_Integer>) {
                        lua_pushinteger(state, key);
                    } else {
                        lua_pushlstring(state, key.data(), key.size());
                    }
                }, entry.key.value);
                entry.value->Push(state);
                lua_settable(state, -3);
            }
        }
    }, m_Value);
}

} // namespace tas::lua
