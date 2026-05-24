#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "LuaRuntime/LuaHeaders.h"
#include "Result.h"

namespace tas::lua {

class LuaValue {
public:
    struct Table;
    using TablePtr = std::shared_ptr<Table>;
    using Storage = std::variant<std::monostate, bool, lua_Integer, lua_Number, std::string, TablePtr>;

    struct Key {
        std::variant<lua_Integer, std::string> value;
    };

    struct Entry {
        Key key;
        std::shared_ptr<LuaValue> value;
    };

    struct Table {
        std::vector<Entry> entries;
    };

    LuaValue() = default;
    explicit LuaValue(Storage value) : m_Value(std::move(value)) {}

    static Result<LuaValue> FromStack(lua_State *state, int index, int maxDepth = 16);
    void Push(lua_State *state) const;

    bool IsTable() const;
    const LuaValue *FindField(const std::string &key) const;
    std::string GetStringField(const std::string &key, std::string defaultValue) const;
    lua_Number GetNumberField(const std::string &key, lua_Number defaultValue) const;
    lua_Integer GetIntegerField(const std::string &key, lua_Integer defaultValue) const;
    bool GetBoolField(const std::string &key, bool defaultValue) const;

private:
    static Result<LuaValue> FromStackImpl(lua_State *state, int index, int remainingDepth);

    Storage m_Value;
};

} // namespace tas::lua
