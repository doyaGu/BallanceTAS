#include "LuaApi.h"

#include "HarnessRuntime.h"
#include "ScriptContext.h"
#include "ServiceContainer.h"
#include "TASEngine.h"

namespace {
ScriptContext *GetContext(lua_State *state) {
    return static_cast<ScriptContext *>(lua_touserdata(state, lua_upvalueindex(1)));
}

HarnessRuntime *GetHarness(lua_State *state) {
    auto *context = GetContext(state);
    auto *engine = context ? context->GetEngine() : nullptr;
    return engine ? engine->GetServiceProvider().Resolve<HarnessRuntime>() : nullptr;
}

int IsActive(lua_State *state) {
    auto *harness = GetHarness(state);
    lua_pushboolean(state, harness && harness->IsActive() && !harness->IsCompleted());
    return 1;
}

int Complete(lua_State *state, bool passed) {
    const int count = lua_gettop(state);
    if (count > 1 || (count == 1 && !lua_isstring(state, 1))) {
        return luaL_error(state, "tas.harness.%s([message]): expected zero or one string argument",
                          passed ? "pass" : "fail");
    }

    auto *harness = GetHarness(state);
    if (!harness || !harness->IsActive()) {
        return luaL_error(state, "harness mode is not active");
    }

    const char *message = count == 1 ? lua_tostring(state, 1) : nullptr;
    const std::string text = message ? message : (passed ? "completed" : "failed");
    if (!harness->Complete(passed, text)) {
        return luaL_error(state, "failed to publish harness result");
    }
    return 0;
}

int Pass(lua_State *state) { return Complete(state, true); }
int Fail(lua_State *state) { return Complete(state, false); }

void SetFunction(lua_State *state, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(state, context);
    lua_pushcclosure(state, function, 1);
    lua_setfield(state, -2, name);
}
}

void LuaApi::RegisterHarnessApi(lua_State *state, ScriptContext *context) {
    lua_getglobal(state, "tas");
    lua_newtable(state);
    SetFunction(state, "active", IsActive, context);
    SetFunction(state, "pass", Pass, context);
    SetFunction(state, "fail", Fail, context);
    lua_setfield(state, -2, "harness");
    lua_pop(state, 1);
}
