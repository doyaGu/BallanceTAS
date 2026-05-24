#include "AsyncTask.h"

#include <utility>

AsyncTask::AsyncTask(LuaScheduler *scheduler, tas::lua::LuaThread coroutine, ScriptContext *context)
    : m_Scheduler(scheduler),
      m_Context(context),
      m_Coroutine(std::make_shared<tas::lua::LuaThread>(std::move(coroutine))) {}

void AsyncTask::Start() {
    if (m_State != AsyncTaskState::Pending) {
        return; // Already started
    }

    m_State = AsyncTaskState::Running;
    m_CoroutineId = 1; // Mark as started
}

void AsyncTask::Cancel() {
    if (IsDone()) {
        return; // Already done
    }

    m_State = AsyncTaskState::Cancelled;
    m_CoroutineId = -1; // Mark as cancelled
}

bool AsyncTask::Poll() {
    if (IsDone()) {
        return false; // Done, no more polling needed
    }

    if (m_State == AsyncTaskState::Pending) {
        Start();
    }

    if (!m_Coroutine || !m_Coroutine->IsValid()) {
        SetError("AsyncTask coroutine is invalid");
        return false;
    }

    auto result = m_Coroutine->Resume();
    if (result.IsError()) {
        SetError(result.GetError().message);
        return false;
    }

    if (m_Coroutine->Status() == tas::lua::LuaThreadStatus::Yielded) {
        return true;
    }

    if (m_State == AsyncTaskState::Running) {
            lua_State *L = m_Coroutine->State();
        if (L && result.Unwrap() > 0) {
            auto value = tas::lua::LuaValue::FromStack(L, -1);
            if (value.IsOk()) {
                m_Result = value.Unwrap();
            }
            lua_pop(L, result.Unwrap());
        }
        m_State = AsyncTaskState::Completed;
    }
    return false;
}

void AsyncTask::SetResult(tas::lua::LuaValue result) {
    m_Result = std::move(result);
    m_State = AsyncTaskState::Completed;
}

void AsyncTask::SetError(const std::string &error) {
    m_Error = error;
    m_State = AsyncTaskState::Failed;
}
