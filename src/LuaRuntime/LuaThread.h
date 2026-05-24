#pragma once

#include "LuaRuntime/LuaHeaders.h"
#include "LuaRuntime/LuaRef.h"
#include "Result.h"

namespace tas::lua {

enum class LuaThreadStatus {
    Runnable,
    Yielded,
    Dead,
    Error
};

class LuaThread {
public:
    LuaThread() = default;
    ~LuaThread() = default;

    LuaThread(const LuaThread &) = delete;
    LuaThread &operator=(const LuaThread &) = delete;
    LuaThread(LuaThread &&) noexcept = default;
    LuaThread &operator=(LuaThread &&) noexcept = default;

    static LuaThread CreateFromFunction(lua_State *mainState, int functionIndex);

    bool IsValid() const { return m_State != nullptr && m_ThreadRef.IsValid(); }
    lua_State *State() const { return m_State; }
    LuaThreadStatus Status() const;

    Result<int> Resume(int argCount = 0, lua_State *from = nullptr);
    void Reset();

private:
    LuaThread(lua_State *threadState, LuaRef threadRef)
        : m_State(threadState), m_ThreadRef(std::move(threadRef)) {}

    static std::string BuildTraceback(lua_State *threadState);

    lua_State *m_State = nullptr;
    LuaRef m_ThreadRef;
    bool m_Completed = false;
    bool m_HadError = false;
};

} // namespace tas::lua
