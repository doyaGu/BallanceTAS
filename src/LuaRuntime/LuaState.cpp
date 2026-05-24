#include "LuaRuntime/LuaState.h"

#include <utility>

namespace tas::lua {

LuaState::LuaState() : m_State(luaL_newstate()) {
    if (!m_State) {
        throw std::runtime_error("failed to create Lua state");
    }
    lua_atpanic(m_State, &LuaState::Panic);
}

LuaState::~LuaState() {
    if (m_State) {
        lua_close(m_State);
        m_State = nullptr;
    }
}

LuaState::LuaState(LuaState &&other) noexcept : m_State(std::exchange(other.m_State, nullptr)) {}

LuaState &LuaState::operator=(LuaState &&other) noexcept {
    if (this == &other) {
        return *this;
    }
    if (m_State) {
        lua_close(m_State);
    }
    m_State = std::exchange(other.m_State, nullptr);
    return *this;
}

lua_State *LuaState::MainThread() const {
    if (!m_State) {
        return nullptr;
    }
    lua_rawgeti(m_State, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
    lua_State *mainThread = lua_tothread(m_State, -1);
    lua_pop(m_State, 1);
    return mainThread ? mainThread : m_State;
}

void LuaState::OpenStandardLibraries() {
    luaL_openlibs(m_State);
}

Result<void> LuaState::LoadString(const std::string &script, const std::string &chunkName) {
    const std::string displayName = chunkName.empty() ? "chunk" : chunkName;
    const int status = luaL_loadbufferx(m_State, script.data(), script.size(), displayName.c_str(), nullptr);
    if (status != LUA_OK) {
        std::string message = lua_tostring(m_State, -1) ? lua_tostring(m_State, -1) : "failed to load Lua chunk";
        lua_pop(m_State, 1);
        return Result<void>::Error(std::move(message), "lua.load");
    }
    return Result<void>::Ok();
}

Result<void> LuaState::LoadFile(const std::string &path) {
    const int status = luaL_loadfilex(m_State, path.c_str(), nullptr);
    if (status != LUA_OK) {
        std::string message = lua_tostring(m_State, -1) ? lua_tostring(m_State, -1) : "failed to load Lua file";
        lua_pop(m_State, 1);
        return Result<void>::Error(std::move(message), "lua.load");
    }
    return Result<void>::Ok();
}

int LuaState::Traceback(lua_State *state) {
    const char *message = lua_tostring(state, 1);
    if (message) {
        luaL_traceback(state, state, message, 1);
    } else if (!lua_isnoneornil(state, 1)) {
        if (!luaL_callmeta(state, 1, "__tostring")) {
            lua_pushliteral(state, "(non-string Lua error)");
        }
    } else {
        lua_pushliteral(state, "(nil Lua error)");
    }
    return 1;
}

int LuaState::Panic(lua_State *state) {
    const char *message = lua_tostring(state, -1);
    throw std::runtime_error(message ? message : "Lua panic");
}

} // namespace tas::lua
