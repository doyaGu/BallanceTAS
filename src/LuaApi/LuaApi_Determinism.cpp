#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"

#include "DeterminismTrace.h"
#include "DeterminismVerifier.h"
#include "ScriptContext.h"
#include "TASEngine.h"
#include "TASConstants.h"
#include "TASProject.h"

#include <cstdio>
#include <filesystem>

static DeterminismVerifier *GetVerifier(lua_State *L) {
    auto *context = static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
    return context ? context->GetDeterminismVerifier() : nullptr;
}

static ScriptContext *GetContext(lua_State *L) {
    return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

static std::filesystem::path ProjectTraceDirectory(const TASProject *project) {
    if (!project) {
        return {};
    }

    std::filesystem::path path(project->GetPath());
    if (project->IsRecordProject() || path.has_extension()) {
        return path.parent_path();
    }
    return path;
}

static Result<std::filesystem::path> ResolveTracePath(lua_State *L, int index) {
    auto *context = GetContext(L);
    auto *project = context ? context->GetCurrentProject() : nullptr;
    tas::determinism::TracePathRequest request;
    if (!lua_isnoneornil(L, index)) {
        request.requestedPath = std::filesystem::path(luaL_checkstring(L, index));
    }
    request.projectDirectory = ProjectTraceDirectory(project);
    request.projectName = project ? project->GetName() : "trace";
    request.tasRoot = (context && context->GetEngine()) ? context->GetEngine()->GetPath() : TASConstants::DefaultBasePath;
    return tas::determinism::ResolveTracePath(request);
}

static void PushResultErrorOrNil(lua_State *L, const Result<void> &result) {
    if (result.IsOk()) {
        lua_pushnil(L);
        return;
    }
    const auto &error = result.GetError();
    lua_pushlstring(L, error.message.data(), error.message.size());
}

static void PushStatus(lua_State *L, const DeterminismVerifier::Status &status) {
    lua_newtable(L);

    const char *mode = "idle";
    switch (status.mode) {
    case DeterminismVerifier::Mode::Idle:
        mode = "idle";
        break;
    case DeterminismVerifier::Mode::Recording:
        mode = "recording";
        break;
    case DeterminismVerifier::Mode::Verifying:
        mode = "verifying";
        break;
    }

    lua_pushstring(L, mode);
    lua_setfield(L, -2, "mode");
    lua_pushinteger(L, static_cast<lua_Integer>(status.ticksProcessed));
    lua_setfield(L, -2, "ticks_processed");
    lua_pushboolean(L, status.diverged);
    lua_setfield(L, -2, "diverged");
    if (status.diverged) {
        lua_pushinteger(L, static_cast<lua_Integer>(status.divergenceTick));
    } else {
        lua_pushnil(L);
    }
    lua_setfield(L, -2, "divergence_tick");
    lua_pushinteger(L, static_cast<lua_Integer>(status.lastHash));
    lua_setfield(L, -2, "last_hash");
    lua_pushlstring(L, status.currentPath.data(), status.currentPath.size());
    lua_setfield(L, -2, "current_path");
}

static void SetDeterminismFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, function, 1);
    lua_setfield(L, -2, name);
}

static int StartRecording(lua_State *L) {
    auto *verifier = GetVerifier(L);
    if (!verifier) {
        lua_pushstring(L, "DeterminismVerifier not available");
        return 1;
    }
    auto path = ResolveTracePath(L, 1);
    if (path.IsError()) {
        PushResultErrorOrNil(L, Result<void>::Error(path.GetError().message, path.GetError().category));
        return 1;
    }
    PushResultErrorOrNil(L, verifier->StartRecording(path.Unwrap().string()));
    return 1;
}

static int StartVerification(lua_State *L) {
    auto *verifier = GetVerifier(L);
    if (!verifier) {
        lua_pushstring(L, "DeterminismVerifier not available");
        return 1;
    }
    auto path = ResolveTracePath(L, 1);
    if (path.IsError()) {
        PushResultErrorOrNil(L, Result<void>::Error(path.GetError().message, path.GetError().category));
        return 1;
    }
    PushResultErrorOrNil(L, verifier->StartVerification(path.Unwrap().string()));
    return 1;
}

static int Stop(lua_State *L) {
    auto *verifier = GetVerifier(L);
    if (verifier) {
        verifier->Stop();
    }
    return 0;
}

static int GetCurrentHash(lua_State *L) {
    auto *verifier = GetVerifier(L);
    char buffer[19];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "0x%016llX",
                  static_cast<unsigned long long>(verifier ? verifier->GetCurrentHash() : 0));
    lua_pushstring(L, buffer);
    return 1;
}

static int GetStatus(lua_State *L) {
    auto *verifier = GetVerifier(L);
    PushStatus(L, verifier ? verifier->GetStatus() : DeterminismVerifier::Status{});
    return 1;
}

static int OfflineDiff(lua_State *L) {
    const char *pathA = luaL_checkstring(L, 1);
    const char *pathB = luaL_checkstring(L, 2);
    auto result = DeterminismVerifier::OfflineDiff(pathA ? pathA : "", pathB ? pathB : "");
    if (result.IsError()) {
        PushResultErrorOrNil(L, Result<void>::Error(result.GetError().message, result.GetError().category));
        return 1;
    }

    const auto diff = result.Unwrap();
    lua_createtable(L, 0, 6);
    lua_pushboolean(L, diff.identical);
    lua_setfield(L, -2, "identical");
    lua_pushinteger(L, static_cast<lua_Integer>(diff.firstDivergenceTick));
    lua_setfield(L, -2, "first_divergence_tick");
    lua_pushinteger(L, static_cast<lua_Integer>(diff.divergentTicks));
    lua_setfield(L, -2, "divergent_ticks");
    lua_pushinteger(L, static_cast<lua_Integer>(diff.comparedTicks));
    lua_setfield(L, -2, "compared_ticks");
    lua_pushinteger(L, static_cast<lua_Integer>(diff.leftTicks));
    lua_setfield(L, -2, "left_ticks");
    lua_pushinteger(L, static_cast<lua_Integer>(diff.rightTicks));
    lua_setfield(L, -2, "right_ticks");
    return 1;
}

void LuaApi::RegisterDeterminismApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);

    lua_getglobal(state, "tas");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_setglobal(state, "tas");
        lua_getglobal(state, "tas");
    }

    lua_newtable(state);
    SetDeterminismFunction(state, "start_recording", StartRecording, context);
    SetDeterminismFunction(state, "start_verification", StartVerification, context);
    SetDeterminismFunction(state, "stop", Stop, context);
    SetDeterminismFunction(state, "get_current_hash", GetCurrentHash, context);
    SetDeterminismFunction(state, "get_status", GetStatus, context);
    SetDeterminismFunction(state, "offline_diff", OfflineDiff, context);
    lua_setfield(state, -2, "determinism");

    lua_pop(state, 1);
}
