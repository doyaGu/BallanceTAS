#pragma once

#include <new>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "LuaRuntime/LuaHeaders.h"

namespace tas::lua {

struct UserdataTypeDescriptor {
    const char *metatableName = nullptr;
    const UserdataTypeDescriptor *base = nullptr;
    void *(*castToBase)(void *) = nullptr;
};

struct UserdataHeader {
    void *ptr = nullptr;
    bool owned = false;
    const UserdataTypeDescriptor *type = nullptr;
};

template <typename T>
struct UserdataBox {
    T *ptr = nullptr;
    bool owned = false;
    const UserdataTypeDescriptor *type = nullptr;
};

template <typename T>
UserdataTypeDescriptor *TypeDescriptorStorage() {
    static UserdataTypeDescriptor descriptor;
    return &descriptor;
}

template <typename T>
const char *&MetatableNameStorage() {
    static const char *name = nullptr;
    return name;
}

inline std::unordered_map<std::string, UserdataTypeDescriptor *> &UserdataTypeRegistry() {
    static std::unordered_map<std::string, UserdataTypeDescriptor *> registry;
    return registry;
}

template <typename T>
UserdataTypeDescriptor *GetOrCreateTypeDescriptor(const char *metatableName) {
    auto *descriptor = TypeDescriptorStorage<T>();
    descriptor->metatableName = metatableName;
    UserdataTypeRegistry()[metatableName] = descriptor;
    MetatableNameStorage<T>() = metatableName;
    return descriptor;
}

inline void *CastUserdataPointer(void *ptr,
                                 const UserdataTypeDescriptor *actual,
                                 const UserdataTypeDescriptor *target) {
    if (!ptr || !actual || !target) {
        return nullptr;
    }
    if (actual == target) {
        return ptr;
    }
    const UserdataTypeDescriptor *current = actual;
    void *currentPtr = ptr;
    while (current && current->base && current->castToBase) {
        currentPtr = current->castToBase(currentPtr);
        current = current->base;
        if (current == target) {
            return currentPtr;
        }
    }
    return nullptr;
}

template <typename T>
int UserdataGc(lua_State *state) {
    auto *box = static_cast<UserdataBox<T> *>(lua_touserdata(state, 1));
    if (box && box->owned && box->ptr) {
        delete box->ptr;
        box->ptr = nullptr;
        box->owned = false;
    }
    return 0;
}

template <typename T>
void RegisterUserdata(lua_State *state, const char *metatableName) {
    auto *descriptor = GetOrCreateTypeDescriptor<T>(metatableName);
    if (luaL_newmetatable(state, metatableName)) {
        lua_pushcfunction(state, &UserdataGc<T>);
        lua_setfield(state, -2, "__gc");
        lua_pushvalue(state, -1);
        lua_setfield(state, -2, "__index");
    }
    lua_pushlightuserdata(state, descriptor);
    lua_setfield(state, -2, "__tas_type_descriptor");
    lua_pop(state, 1);
}

template <typename T, typename... Args>
void PushOwnedUserdata(lua_State *state, const char *metatableName, Args &&...args) {
    auto *box = static_cast<UserdataBox<T> *>(lua_newuserdatauv(state, sizeof(UserdataBox<T>), 0));
    new (box) UserdataBox<T>{new T(std::forward<Args>(args)...), true, GetOrCreateTypeDescriptor<T>(metatableName)};
    luaL_getmetatable(state, metatableName);
    lua_setmetatable(state, -2);
}

template <typename T>
void PushBorrowedUserdata(lua_State *state, const char *metatableName, T *ptr) {
    auto *box = static_cast<UserdataBox<T> *>(lua_newuserdatauv(state, sizeof(UserdataBox<T>), 0));
    new (box) UserdataBox<T>{ptr, false, GetOrCreateTypeDescriptor<T>(metatableName)};
    luaL_getmetatable(state, metatableName);
    lua_setmetatable(state, -2);
}

template <typename T>
T *CheckUserdata(lua_State *state, int index, const char *metatableName) {
    auto *target = GetOrCreateTypeDescriptor<T>(metatableName);
    if (auto *box = static_cast<UserdataBox<T> *>(luaL_testudata(state, index, metatableName))) {
        if (!box->ptr) {
            luaL_error(state, "%s userdata pointer is null", metatableName);
            return nullptr;
        }
        return box->ptr;
    }

    auto *header = static_cast<UserdataHeader *>(lua_touserdata(state, index));
    if (header && lua_getmetatable(state, index)) {
        lua_getfield(state, -1, "__tas_type_descriptor");
        auto *actual = static_cast<UserdataTypeDescriptor *>(lua_touserdata(state, -1));
        lua_pop(state, 2);
        if (!header->ptr) {
            luaL_error(state, "%s userdata pointer is null", actual && actual->metatableName ? actual->metatableName : metatableName);
            return nullptr;
        }
        if (void *cast = CastUserdataPointer(header->ptr, actual ? actual : header->type, target)) {
            return static_cast<T *>(cast);
        }
    }

    luaL_typeerror(state, index, metatableName);
    return nullptr;
}

namespace detail {

constexpr const char *kMethodsField = "__tas_methods";
constexpr const char *kGettersField = "__tas_getters";
constexpr const char *kSettersField = "__tas_setters";
constexpr const char *kNumericGetterField = "__tas_numeric_getter";
constexpr const char *kNumericSetterField = "__tas_numeric_setter";
constexpr const char *kBaseMetatableField = "__tas_base_metatable";

template <typename>
inline constexpr bool always_false_v = false;

template <typename T>
T *UncheckedUserdataValue(lua_State *state, int index) {
    const char *metatableName = MetatableNameStorage<T>();
    if (!metatableName) {
        luaL_error(state, "unregistered userdata type");
        return nullptr;
    }
    return CheckUserdata<T>(state, index, metatableName);
}

template <typename T>
void PushCppValue(lua_State *state, T value) {
    using Value = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<Value, bool>) {
        lua_pushboolean(state, value);
    } else if constexpr (std::is_integral_v<Value>) {
        lua_pushinteger(state, static_cast<lua_Integer>(value));
    } else if constexpr (std::is_floating_point_v<Value>) {
        lua_pushnumber(state, static_cast<lua_Number>(value));
    } else {
        static_assert(always_false_v<Value>, "unsupported Lua userdata property type");
    }
}

template <typename T>
T CheckCppValue(lua_State *state, int index) {
    using Value = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<Value, bool>) {
        return lua_toboolean(state, index) != 0;
    } else if constexpr (std::is_integral_v<Value>) {
        return static_cast<T>(luaL_checkinteger(state, index));
    } else if constexpr (std::is_floating_point_v<Value>) {
        return static_cast<T>(luaL_checknumber(state, index));
    } else {
        static_assert(always_false_v<Value>, "unsupported Lua userdata property type");
    }
}

inline void EnsureSubtable(lua_State *state, int metatableIndex, const char *fieldName) {
    metatableIndex = lua_absindex(state, metatableIndex);
    lua_getfield(state, metatableIndex, fieldName);
    if (lua_istable(state, -1)) {
        lua_pop(state, 1);
        return;
    }
    lua_pop(state, 1);
    lua_newtable(state);
    lua_setfield(state, metatableIndex, fieldName);
}

inline void SetFunctionInSubtable(lua_State *state, int metatableIndex, const char *fieldName, const char *name, lua_CFunction function) {
    metatableIndex = lua_absindex(state, metatableIndex);
    EnsureSubtable(state, metatableIndex, fieldName);
    lua_getfield(state, metatableIndex, fieldName);
    lua_pushcfunction(state, function);
    lua_setfield(state, -2, name);
    lua_pop(state, 1);
}

inline int UserdataIndexDispatch(lua_State *state) {
    if (!lua_getmetatable(state, 1)) {
        lua_pushnil(state);
        return 1;
    }
    const int metatable = lua_gettop(state);

    if (lua_isinteger(state, 2)) {
        lua_getfield(state, metatable, kNumericGetterField);
        if (lua_isfunction(state, -1)) {
            lua_pushvalue(state, 1);
            lua_pushvalue(state, 2);
            lua_call(state, 2, 1);
            return 1;
        }
        lua_pop(state, 1);
    }

    if (lua_type(state, 2) == LUA_TSTRING) {
        auto tryStringIndex = [&](auto &&self, int currentMetatable) -> bool {
            lua_getfield(state, currentMetatable, kGettersField);
            if (lua_istable(state, -1)) {
                lua_pushvalue(state, 2);
                lua_rawget(state, -2);
                if (lua_isfunction(state, -1)) {
                    lua_pushvalue(state, 1);
                    lua_call(state, 1, 1);
                    return true;
                }
                lua_pop(state, 1);
            }
            lua_pop(state, 1);

            lua_getfield(state, currentMetatable, kMethodsField);
            if (lua_istable(state, -1)) {
                lua_pushvalue(state, 2);
                lua_rawget(state, -2);
                if (!lua_isnil(state, -1)) {
                    return true;
                }
                lua_pop(state, 1);
            }
            lua_pop(state, 1);

            lua_getfield(state, currentMetatable, kBaseMetatableField);
            if (lua_istable(state, -1)) {
                const int baseMetatable = lua_gettop(state);
                if (self(self, baseMetatable)) {
                    return true;
                }
            }
            lua_pop(state, 1);
            return false;
        };

        if (tryStringIndex(tryStringIndex, metatable)) {
            return 1;
        }
    }

    lua_pushnil(state);
    return 1;
}

inline int UserdataNewIndexDispatch(lua_State *state) {
    if (!lua_getmetatable(state, 1)) {
        return luaL_error(state, "invalid userdata");
    }
    const int metatable = lua_gettop(state);

    if (lua_isinteger(state, 2)) {
        lua_getfield(state, metatable, kNumericSetterField);
        if (lua_isfunction(state, -1)) {
            lua_pushvalue(state, 1);
            lua_pushvalue(state, 2);
            lua_pushvalue(state, 3);
            lua_call(state, 3, 0);
            return 0;
        }
        lua_pop(state, 1);
    }

    if (lua_type(state, 2) == LUA_TSTRING) {
        auto tryStringNewIndex = [&](auto &&self, int currentMetatable) -> int {
            lua_getfield(state, currentMetatable, kSettersField);
            if (lua_istable(state, -1)) {
                lua_pushvalue(state, 2);
                lua_rawget(state, -2);
                if (lua_isfunction(state, -1)) {
                    lua_pushvalue(state, 1);
                    lua_pushvalue(state, 3);
                    lua_call(state, 2, 0);
                    return 1;
                }
                lua_pop(state, 1);
            }
            lua_pop(state, 1);

            lua_getfield(state, currentMetatable, kGettersField);
            if (lua_istable(state, -1)) {
                lua_pushvalue(state, 2);
                lua_rawget(state, -2);
                if (lua_isfunction(state, -1)) {
                    return luaL_error(state, "field '%s' is read-only", lua_tostring(state, 2));
                }
                lua_pop(state, 1);
            }
            lua_pop(state, 1);

            lua_getfield(state, currentMetatable, kBaseMetatableField);
            if (lua_istable(state, -1)) {
                const int baseMetatable = lua_gettop(state);
                const int handled = self(self, baseMetatable);
                if (handled != 0) {
                    return handled;
                }
            }
            lua_pop(state, 1);
            return 0;
        };

        const int handled = tryStringNewIndex(tryStringNewIndex, metatable);
        if (handled != 0) {
            return handled > 0 ? 0 : handled;
        }
    }

    return luaL_error(state, "unknown userdata field");
}

} // namespace detail

template <typename T>
class LuaUserdataRegistry {
public:
    LuaUserdataRegistry(lua_State *state, const char *metatableName) : m_State(state), m_MetatableName(metatableName) {
        RegisterUserdata<T>(m_State, m_MetatableName);
        luaL_getmetatable(m_State, m_MetatableName);
        const int metatable = lua_gettop(m_State);

        detail::EnsureSubtable(m_State, metatable, detail::kMethodsField);
        detail::EnsureSubtable(m_State, metatable, detail::kGettersField);
        detail::EnsureSubtable(m_State, metatable, detail::kSettersField);

        lua_pushcfunction(m_State, &detail::UserdataIndexDispatch);
        lua_setfield(m_State, metatable, "__index");
        lua_pushcfunction(m_State, &detail::UserdataNewIndexDispatch);
        lua_setfield(m_State, metatable, "__newindex");

        lua_pop(m_State, 1);
    }

    LuaUserdataRegistry &Method(const char *name, lua_CFunction function) {
        WithMetatable([&](int metatable) {
            detail::SetFunctionInSubtable(m_State, metatable, detail::kMethodsField, name, function);
        });
        return *this;
    }

    LuaUserdataRegistry &MetaMethod(const char *name, lua_CFunction function) {
        WithMetatable([&](int metatable) {
            lua_pushcfunction(m_State, function);
            lua_setfield(m_State, metatable, name);
        });
        return *this;
    }

    LuaUserdataRegistry &NumericIndex(lua_CFunction getter, lua_CFunction setter = nullptr) {
        WithMetatable([&](int metatable) {
            lua_pushcfunction(m_State, getter);
            lua_setfield(m_State, metatable, detail::kNumericGetterField);
            if (setter) {
                lua_pushcfunction(m_State, setter);
                lua_setfield(m_State, metatable, detail::kNumericSetterField);
            }
        });
        return *this;
    }

    template <typename BaseT>
    LuaUserdataRegistry &Base(const char *baseMetatableName) {
        RegisterUserdata<BaseT>(m_State, baseMetatableName);
        auto *derived = GetOrCreateTypeDescriptor<T>(m_MetatableName);
        auto *base = GetOrCreateTypeDescriptor<BaseT>(baseMetatableName);
        derived->base = base;
        derived->castToBase = [](void *ptr) -> void * {
            return static_cast<BaseT *>(static_cast<T *>(ptr));
        };

        WithMetatable([&](int metatable) {
            luaL_getmetatable(m_State, baseMetatableName);
            lua_setfield(m_State, metatable, detail::kBaseMetatableField);
        });
        return *this;
    }

    LuaUserdataRegistry &Property(const char *name, lua_CFunction getter, lua_CFunction setter) {
        WithMetatable([&](int metatable) {
            detail::SetFunctionInSubtable(m_State, metatable, detail::kGettersField, name, getter);
            detail::SetFunctionInSubtable(m_State, metatable, detail::kSettersField, name, setter);
        });
        return *this;
    }

    LuaUserdataRegistry &ReadonlyProperty(const char *name, lua_CFunction getter) {
        WithMetatable([&](int metatable) {
            detail::SetFunctionInSubtable(m_State, metatable, detail::kGettersField, name, getter);
        });
        return *this;
    }

    template <auto Member>
    LuaUserdataRegistry &Property(const char *name) {
        WithMetatable([&](int metatable) {
            detail::SetFunctionInSubtable(m_State, metatable, detail::kGettersField, name, &MemberGetter<Member>);
            detail::SetFunctionInSubtable(m_State, metatable, detail::kSettersField, name, &MemberSetter<Member>);
        });
        return *this;
    }

    template <auto Member>
    LuaUserdataRegistry &ReadonlyProperty(const char *name) {
        WithMetatable([&](int metatable) {
            detail::SetFunctionInSubtable(m_State, metatable, detail::kGettersField, name, &MemberGetter<Member>);
        });
        return *this;
    }

private:
    template <auto Member>
    static int MemberGetter(lua_State *state) {
        T *object = detail::UncheckedUserdataValue<T>(state, 1);
        detail::PushCppValue(state, object->*Member);
        return 1;
    }

    template <auto Member>
    static int MemberSetter(lua_State *state) {
        T *object = detail::UncheckedUserdataValue<T>(state, 1);
        using MemberType = std::remove_reference_t<decltype(object->*Member)>;
        object->*Member = detail::CheckCppValue<MemberType>(state, 2);
        return 0;
    }

    template <typename F>
    void WithMetatable(F &&function) {
        luaL_getmetatable(m_State, m_MetatableName);
        const int metatable = lua_gettop(m_State);
        function(metatable);
        lua_pop(m_State, 1);
    }

    lua_State *m_State = nullptr;
    const char *m_MetatableName = nullptr;
};

} // namespace tas::lua
