#include "LuaApi.h"

#include "../LuaRuntime/LuaRef.h"
#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include "Logger.h"
#include "ScriptContext.h"

#include <string>
#include <vector>

constexpr const char *kResultMt = "BallanceTAS.Result";

struct LuaResult {
    bool isSuccess = false;
    tas::lua::LuaRef value;
    std::string error;

    LuaResult(bool success, tas::lua::LuaRef ref, std::string errorMessage = {})
        : isSuccess(success), value(std::move(ref)), error(std::move(errorMessage)) {}

    LuaResult(LuaResult &&) noexcept = default;
    LuaResult &operator=(LuaResult &&) noexcept = default;
    LuaResult(const LuaResult &) = delete;
    LuaResult &operator=(const LuaResult &) = delete;
};

static LuaResult *CheckResult(lua_State *L, int index) {
    return tas::lua::CheckUserdata<LuaResult>(L, index, kResultMt);
}

static LuaResult *TestResult(lua_State *L, int index) {
    auto *box = static_cast<tas::lua::UserdataBox<LuaResult> *>(luaL_testudata(L, index, kResultMt));
    return box ? box->ptr : nullptr;
}

static void PushOkFromStack(lua_State *L, int index) {
    tas::lua::PushOwnedUserdata<LuaResult>(L, kResultMt, true, tas::lua::LuaRef::FromStack(L, index), std::string{});
}

static void PushOkRef(lua_State *L, tas::lua::LuaRef ref) {
    tas::lua::PushOwnedUserdata<LuaResult>(L, kResultMt, true, std::move(ref), std::string{});
}

static void PushErr(lua_State *L, std::string error) {
    tas::lua::PushOwnedUserdata<LuaResult>(L, kResultMt, false, tas::lua::LuaRef{}, std::move(error));
}

static void PushResultValue(lua_State *L, const LuaResult &result) {
    if (!result.value.State()) {
        lua_pushnil(L);
        return;
    }
    result.value.Push(L);
}

static std::string ToErrorString(lua_State *L, int index) {
    const char *text = lua_tostring(L, index);
    return text ? text : "<non-string Lua error>";
}

static bool ProtectedCall(lua_State *L, int nargs, int nresults, std::string &error) {
    if (lua_pcall(L, nargs, nresults, 0) == LUA_OK) {
        return true;
    }
    error = ToErrorString(L, -1);
    lua_pop(L, 1);
    return false;
}

static int IsOk(lua_State *L) {
    lua_pushboolean(L, CheckResult(L, 1)->isSuccess);
    return 1;
}

static int IsErr(lua_State *L) {
    lua_pushboolean(L, !CheckResult(L, 1)->isSuccess);
    return 1;
}

static int Unwrap(lua_State *L) {
    auto *result = CheckResult(L, 1);
    if (!result->isSuccess) {
        return luaL_error(L, "Called unwrap() on Err result: %s", result->error.c_str());
    }
    PushResultValue(L, *result);
    return 1;
}

static int UnwrapOr(lua_State *L) {
    auto *result = CheckResult(L, 1);
    if (result->isSuccess) {
        PushResultValue(L, *result);
    } else {
        lua_pushvalue(L, 2);
    }
    return 1;
}

static int UnwrapOrElse(lua_State *L) {
    auto *result = CheckResult(L, 1);
    if (result->isSuccess) {
        PushResultValue(L, *result);
        return 1;
    }
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    std::string error;
    if (!ProtectedCall(L, 0, 1, error)) {
        return luaL_error(L, "unwrap_or_else function failed: %s", error.c_str());
    }
    return 1;
}

static int Expect(lua_State *L) {
    auto *result = CheckResult(L, 1);
    if (!result->isSuccess) {
        return luaL_error(L, "%s: %s", luaL_checkstring(L, 2), result->error.c_str());
    }
    PushResultValue(L, *result);
    return 1;
}

static int Error(lua_State *L) {
    auto *result = CheckResult(L, 1);
    if (result->isSuccess) {
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, result->error.data(), result->error.size());
    }
    return 1;
}

static int Map(lua_State *L) {
    auto *result = CheckResult(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (!result->isSuccess) {
        PushErr(L, result->error);
        return 1;
    }
    lua_pushvalue(L, 2);
    PushResultValue(L, *result);
    std::string error;
    if (!ProtectedCall(L, 1, 1, error)) {
        PushErr(L, "map function failed: " + error);
        return 1;
    }
    PushOkFromStack(L, -1);
    return 1;
}

static int MapErr(lua_State *L) {
    auto *result = CheckResult(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (result->isSuccess) {
        PushOkRef(L, result->value.Clone());
        return 1;
    }
    lua_pushvalue(L, 2);
    lua_pushlstring(L, result->error.data(), result->error.size());
    std::string error;
    if (!ProtectedCall(L, 1, 1, error)) {
        PushErr(L, "map_err function failed: " + error);
        return 1;
    }
    if (lua_type(L, -1) == LUA_TSTRING) {
        PushErr(L, lua_tostring(L, -1));
        lua_pop(L, 1);
        return 1;
    }
    lua_pop(L, 1);
    PushErr(L, result->error);
    return 1;
}

static int AndThen(lua_State *L) {
    auto *result = CheckResult(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (!result->isSuccess) {
        PushErr(L, result->error);
        return 1;
    }
    lua_pushvalue(L, 2);
    PushResultValue(L, *result);
    std::string error;
    if (!ProtectedCall(L, 1, 1, error)) {
        PushErr(L, "and_then function failed: " + error);
        return 1;
    }
    if (TestResult(L, -1)) {
        return 1;
    }
    PushOkFromStack(L, -1);
    return 1;
}

static int OrElse(lua_State *L) {
    auto *result = CheckResult(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (result->isSuccess) {
        PushOkRef(L, result->value.Clone());
        return 1;
    }
    lua_pushvalue(L, 2);
    lua_pushlstring(L, result->error.data(), result->error.size());
    std::string error;
    if (!ProtectedCall(L, 1, 1, error)) {
        PushErr(L, "or_else function failed: " + error);
        return 1;
    }
    if (TestResult(L, -1)) {
        return 1;
    }
    PushOkFromStack(L, -1);
    return 1;
}

static int Match(lua_State *L) {
    auto *result = CheckResult(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_getfield(L, 2, result->isSuccess ? "ok" : "err");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        if (result->isSuccess) {
            PushResultValue(L, *result);
        } else {
            lua_pushnil(L);
        }
        return 1;
    }
    if (result->isSuccess) {
        PushResultValue(L, *result);
    } else {
        lua_pushlstring(L, result->error.data(), result->error.size());
    }
    std::string error;
    if (!ProtectedCall(L, 1, 1, error)) {
        return luaL_error(L, "result.match handler failed: %s", error.c_str());
    }
    return 1;
}

static int ToString(lua_State *L) {
    auto *result = CheckResult(L, 1);
    if (result->isSuccess) {
        lua_pushliteral(L, "Result.Ok");
    } else {
        lua_pushfstring(L, "Result.Err(%s)", result->error.c_str());
    }
    return 1;
}

static int Ok(lua_State *L) {
    if (lua_gettop(L) < 1) {
        lua_pushnil(L);
    }
    PushOkFromStack(L, 1);
    return 1;
}

static int Err(lua_State *L) {
    PushErr(L, luaL_checkstring(L, 1));
    return 1;
}

static int Try(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    std::string error;
    if (ProtectedCall(L, 0, 1, error)) {
        PushOkFromStack(L, -1);
        return 1;
    }
    Log::Warn("Result.try caught error: %s", error.c_str());
    PushErr(L, error);
    return 1;
}

static int All(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_newtable(L);
    int outputIndex = 1;
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        auto *result = TestResult(L, -1);
        if (!result) {
            lua_pop(L, 2);
            PushErr(L, "result.all: all elements must be Result");
            return 1;
        }
        if (!result->isSuccess) {
            const std::string error = result->error;
            lua_pop(L, 3);
            PushErr(L, error);
            return 1;
        }
        PushResultValue(L, *result);
        lua_rawseti(L, -4, outputIndex++);
        lua_pop(L, 1);
    }
    PushOkFromStack(L, -1);
    return 1;
}

static int Any(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    std::vector<std::string> errors;
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        auto *result = TestResult(L, -1);
        if (!result) {
            lua_pop(L, 2);
            PushErr(L, "result.any: all elements must be Result");
            return 1;
        }
        if (result->isSuccess) {
            PushOkRef(L, result->value.Clone());
            lua_remove(L, -2);
            return 1;
        }
        errors.push_back(result->error);
        lua_pop(L, 1);
    }

    std::string combined = "result.any: all results failed:\n";
    for (size_t i = 0; i < errors.size(); ++i) {
        combined += "  [" + std::to_string(i + 1) + "] " + errors[i] + "\n";
    }
    PushErr(L, std::move(combined));
    return 1;
}

static void SetFunction(lua_State *L, const char *name, lua_CFunction function) {
    lua_pushcfunction(L, function);
    lua_setfield(L, -2, name);
}

static void RegisterResultTable(lua_State *L) {
    lua_getglobal(L, "tas");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "tas");
    }

    lua_newtable(L);
    SetFunction(L, "ok", Ok);
    SetFunction(L, "err", Err);
    SetFunction(L, "try", Try);
    SetFunction(L, "all", All);
    SetFunction(L, "any", Any);
    lua_setfield(L, -2, "result");
    lua_pop(L, 1);
}

void LuaApi::RegisterResultApi(lua_State *state, ScriptContext *context) {
    if (!state || !context) {
        throw std::runtime_error("LuaApi::RegisterResultApi requires a valid Lua state and ScriptContext");
    }

    tas::lua::LuaStackGuard guard(state);
    tas::lua::LuaUserdataRegistry<LuaResult>(state, kResultMt)
        .Method("is_ok", IsOk)
        .Method("is_err", IsErr)
        .Method("unwrap", Unwrap)
        .Method("unwrap_or", UnwrapOr)
        .Method("unwrap_or_else", UnwrapOrElse)
        .Method("expect", Expect)
        .Method("error", Error)
        .Method("map", Map)
        .Method("map_err", MapErr)
        .Method("and_then", AndThen)
        .Method("or_else", OrElse)
        .Method("match", Match)
        .MetaMethod("__tostring", ToString);

    RegisterResultTable(state);
}
