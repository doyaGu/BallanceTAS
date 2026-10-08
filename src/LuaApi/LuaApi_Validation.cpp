#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"

#include "PlaybackService.h"
#include "RecordPlayer.h"
#include "ScriptContext.h"
#include "TASEngine.h"
#include "TASProject.h"
#include "ValidationService.h"

#include <filesystem>

static ScriptContext *GetContext(lua_State *L) {
    return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

static ValidationService *GetService(lua_State *L) {
    auto *context = GetContext(L);
    return context ? context->GetValidationService() : nullptr;
}

static PlaybackService *GetPlayback(lua_State *L) {
    auto *context = GetContext(L);
    return (context && context->GetEngine())
               ? context->GetEngine()->GetServiceProvider().Resolve<PlaybackService>()
               : nullptr;
}

static RecordPlayer *GetRecordPlayer(lua_State *L) {
    auto *context = GetContext(L);
    return context ? context->GetRecordPlayer() : nullptr;
}

// Dump/report files land next to the current project, or in the TAS root.
static std::string ResolveOutputPath(ScriptContext *context) {
    if (!context) return "";
    const TASProject *project = context->GetCurrentProject();
    if (project) {
        std::filesystem::path p(project->GetPath());
        if (p.has_extension()) p = p.parent_path();
        if (!p.empty()) {
            std::string s = p.string();
            if (!s.empty() && s.back() != '\\' && s.back() != '/') s.push_back('\\');
            return s;
        }
    }
    auto *engine = context->GetEngine();
    return engine ? engine->GetPath() : "";
}

static void PushErrorOrNil(lua_State *L, const Result<void> &result) {
    if (result.IsOk()) {
        lua_pushnil(L);
        return;
    }
    const auto &error = result.GetError();
    lua_pushlstring(L, error.message.data(), error.message.size());
}

static void PushReportTable(lua_State *L, const ValidationReport &r) {
    lua_newtable(L);
    lua_pushboolean(L, r.hasReference);
    lua_setfield(L, -2, "has_reference");
    lua_pushboolean(L, r.identical);
    lua_setfield(L, -2, "identical");
    lua_pushinteger(L, static_cast<lua_Integer>(r.comparedFrames));
    lua_setfield(L, -2, "compared_frames");
    lua_pushinteger(L, static_cast<lua_Integer>(r.referenceFrames));
    lua_setfield(L, -2, "reference_frames");
    lua_pushinteger(L, static_cast<lua_Integer>(r.liveFrames));
    lua_setfield(L, -2, "live_frames");
    lua_pushinteger(L, static_cast<lua_Integer>(r.divergentFrames));
    lua_setfield(L, -2, "divergent_frames");
    if (r.divergentFrames > 0) {
        lua_pushinteger(L, static_cast<lua_Integer>(r.firstDivergenceFrame));
    } else {
        lua_pushnil(L);
    }
    lua_setfield(L, -2, "first_divergence_frame");
    lua_pushnumber(L, static_cast<lua_Number>(r.maxPositionError));
    lua_setfield(L, -2, "max_position_error");
    lua_pushinteger(L, static_cast<lua_Integer>(r.maxPositionFrame));
    lua_setfield(L, -2, "max_position_frame");
    lua_pushnumber(L, static_cast<lua_Number>(r.maxVelocityError));
    lua_setfield(L, -2, "max_velocity_error");
    lua_pushinteger(L, static_cast<lua_Integer>(r.maxVelocityFrame));
    lua_setfield(L, -2, "max_velocity_frame");
    if (r.dumpPath.empty()) {
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, r.dumpPath.data(), r.dumpPath.size());
    }
    lua_setfield(L, -2, "dump_path");
    if (r.reportPath.empty()) {
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, r.reportPath.data(), r.reportPath.size());
    }
    lua_setfield(L, -2, "report_path");
}

static void SetValidationFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, function, 1);
    lua_setfield(L, -2, name);
}

// tas.validation.start([reference_path])
//   Begins capturing a validation recording during active playback. When a
//   reference trace (a previous validation dump with physics) is supplied, the
//   subsequent stop() compares the live run against it frame-by-frame.
//   Returns nil on success, error string on failure.
static int Start(lua_State *L) {
    auto *service = GetService(L);
    if (!service) {
        lua_pushstring(L, "ValidationService not available");
        return 1;
    }
    auto *playback = GetPlayback(L);
    if (!playback) {
        lua_pushstring(L, "PlaybackService not available");
        return 1;
    }
    auto *recordPlayer = GetRecordPlayer(L);
    if (!playback->IsPlaying() && !(recordPlayer && recordPlayer->IsPlaying())) {
        lua_pushstring(L, "Validation recording requires active playback");
        return 1;
    }

    std::string reference;
    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
        const char *ref = luaL_checkstring(L, 1);
        if (ref) reference = ref;
    }

    std::string outputPath = ResolveOutputPath(GetContext(L));
    PushErrorOrNil(L, service->Start(outputPath, reference));
    return 1;
}

// tas.validation.stop()
//   Stops the validation recording, writes the dump (+ comparison report if a
//   reference was given at start). Returns the report table on success.
static int Stop(lua_State *L) {
    auto *service = GetService(L);
    if (!service) {
        lua_pushstring(L, "ValidationService not available");
        return 1;
    }
    auto result = service->Stop();
    if (result.IsError()) {
        PushErrorOrNil(L, result);
        return 1;
    }
    PushReportTable(L, service->GetLastReport());
    return 1;
}

// tas.validation.get_report()
//   Returns the report table from the most recent stop().
static int GetReport(lua_State *L) {
    auto *service = GetService(L);
    PushReportTable(L, service ? service->GetLastReport() : ValidationReport{});
    return 1;
}

// tas.validation.is_active()
static int IsActive(lua_State *L) {
    auto *service = GetService(L);
    lua_pushboolean(L, service && service->IsActive());
    return 1;
}

void LuaApi::RegisterValidationApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);

    lua_getglobal(state, "tas");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_setglobal(state, "tas");
        lua_getglobal(state, "tas");
    }

    lua_newtable(state);
    SetValidationFunction(state, "start", Start, context);
    SetValidationFunction(state, "stop", Stop, context);
    SetValidationFunction(state, "get_report", GetReport, context);
    SetValidationFunction(state, "is_active", IsActive, context);
    lua_setfield(state, -2, "validation");

    lua_pop(state, 1);
}
