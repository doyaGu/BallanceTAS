#include "LuaApiCommon.h"

#ifdef Yield
#undef Yield
#endif
#include "LuaScheduler.h"
#include "ScriptContext.h"

namespace LuaApiCommon {
    LuaScheduler *RequireScheduler(lua_State *L, const char *where) {
        ScriptContext *context = GetContext(L);
        if (!context) {
            context = GetRegistryContext(L);
        }
        LuaScheduler *scheduler = context ? context->GetScheduler() : nullptr;
        if (!scheduler) {
            Raise(L, where, "the scheduler is not available");
        }
        return scheduler;
    }

    bool CanYield(lua_State *L) {
        ScriptContext *context = GetContext(L);
        if (!context) {
            context = GetRegistryContext(L);
        }
        LuaScheduler *scheduler = context ? context->GetScheduler() : nullptr;
        return scheduler && lua_isyieldable(L) && scheduler->CanYieldCurrentThread();
    }

    LuaScheduler *RequireYieldableScheduler(lua_State *L, const char *where) {
        LuaScheduler *scheduler = RequireScheduler(L, where);
        if (!lua_isyieldable(L) || !scheduler->CanYieldCurrentThread()) {
            Raise(L, where, "waits, so it must be called from a script coroutine (main, a task or a listener)");
        }
        return scheduler;
    }

}
