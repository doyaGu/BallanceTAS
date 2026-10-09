#include "LuaApi.h"

#include "HarnessRuntime.h"
#include "LuaApiCommon.h"
#include "ScriptContext.h"
#include "ServiceContainer.h"
#include "TASEngine.h"

#include <string>

using namespace LuaApiCommon;

namespace {
HarnessRuntime *GetHarness(lua_State *L) {
    ScriptContext *context = GetContext(L);
    TASEngine *engine = context ? context->GetEngine() : nullptr;
    return engine ? engine->GetServiceProvider().Resolve<HarnessRuntime>() : nullptr;
}

int IsActive(lua_State *L) {
    HarnessRuntime *harness = GetHarness(L);
    lua_pushboolean(L, harness && harness->IsActive() && !harness->IsCompleted());
    return 1;
}

// tas.harness.pass/fail([message]) -> true | false, reason
int Complete(lua_State *L, bool passed) {
    const char *message = luaL_optstring(L, 1, passed ? "completed" : "failed");
    HarnessRuntime *harness = GetHarness(L);
    if (!harness || !harness->IsActive()) {
        return PushFail(L, "harness mode is not active");
    }
    const bool published = harness->Complete(passed, std::string(message));
    return published ? PushOk(L) : PushFail(L, "the harness result could not be published");
}

int Pass(lua_State *L) { return Complete(L, true); }
int Fail(lua_State *L) { return Complete(L, false); }
}

void LuaApi::RegisterHarnessApi(lua_State *state, ScriptContext *context) {
    lua_getglobal(state, "tas");
    lua_newtable(state);
    SetClosure(state, "is_active", IsActive, context);
    SetAlias(state, "active", "is_active"); // deprecated
    SetClosure(state, "pass", Pass, context);
    SetClosure(state, "fail", Fail, context);
    lua_setfield(state, -2, "harness");
    lua_pop(state, 1);
}
