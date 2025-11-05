#include "AsyncTask.h"
#include "LuaScheduler.h"
#include "ScriptContext.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

AsyncTask::AsyncTask(LuaScheduler* scheduler, sol::coroutine coroutine, ScriptContext* context)
    : m_Scheduler(scheduler)
    , m_Context(context)
    , m_Coroutine(std::move(coroutine))
    , m_Result(sol::nil)
{
}

void AsyncTask::Start() {
    if (m_State != AsyncTaskState::Pending) {
        return;  // Already started
    }

    m_State = AsyncTaskState::Running;

    // Start coroutine via scheduler
    // The scheduler will manage the coroutine's execution during Tick()
    m_Scheduler->StartCoroutine(m_Coroutine);

    // Store coroutine for tracking (the coroutine itself serves as our ID)
    m_CoroutineId = 1;  // Mark as started
}

void AsyncTask::Cancel() {
    if (IsDone()) {
        return;  // Already done
    }

    m_State = AsyncTaskState::Cancelled;
    m_CoroutineId = -1;  // Mark as cancelled
}

bool AsyncTask::Poll() {
    if (IsDone()) {
        return false;  // Done, no more polling needed
    }

    if (m_State == AsyncTaskState::Pending) {
        Start();
    }

    // Check coroutine status via Lua C API
    lua_State* L = m_Coroutine.lua_state();
    int status = lua_status(L);

    if (status == LUA_OK) {
        // Coroutine finished successfully
        if (m_State == AsyncTaskState::Running) {
            // Try to get return value from coroutine
            sol::state_view lua(L);
            // The result should be on top of the stack if any
            if (lua_gettop(L) > 0) {
                m_Result = sol::stack::get<sol::object>(L, -1);
            }
            m_State = AsyncTaskState::Completed;
        }
        return false;
    }

    if (status == LUA_YIELD) {
        // Still running (yielded), continue polling
        return true;
    }

    // Error state - extract error message
    if (m_State == AsyncTaskState::Running) {
        std::string error_msg = "Coroutine error";

        // Try to get error message from Lua stack
        if (lua_gettop(L) > 0 && lua_type(L, -1) == LUA_TSTRING) {
            const char* err = lua_tostring(L, -1);
            if (err) {
                error_msg = err;
            }
        }

        SetError(error_msg);
    }
    return false;
}

void AsyncTask::SetResult(sol::object result) {
    m_Result = std::move(result);
    m_State = AsyncTaskState::Completed;
}

void AsyncTask::SetError(const std::string& error) {
    m_Error = error;
    m_State = AsyncTaskState::Failed;
}
