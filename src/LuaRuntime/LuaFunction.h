#pragma once

#include <functional>

#include "LuaRuntime/LuaHeaders.h"
#include "LuaRuntime/LuaRef.h"
#include "Result.h"

namespace tas::lua {

class LuaFunction {
public:
    using ArgumentPusher = std::function<void(lua_State *)>;

    LuaFunction() = default;
    ~LuaFunction() = default;

    LuaFunction(const LuaFunction &) = delete;
    LuaFunction &operator=(const LuaFunction &) = delete;
    LuaFunction(LuaFunction &&) noexcept = default;
    LuaFunction &operator=(LuaFunction &&) noexcept = default;

    static LuaFunction FromStack(lua_State *state, int index);

    bool IsValid() const { return m_Ref.IsValid(); }
    lua_State *State() const { return m_Ref.State(); }

    Result<int> Call(int argCount, int resultCount, const ArgumentPusher &pushArguments = {}) const;
    void Push() const { m_Ref.Push(); }
    void Push(lua_State *targetState) const { m_Ref.Push(targetState); }
    void Reset() { m_Ref.Reset(); }

private:
    explicit LuaFunction(LuaRef ref) : m_Ref(std::move(ref)) {}

    LuaRef m_Ref;
};

} // namespace tas::lua
