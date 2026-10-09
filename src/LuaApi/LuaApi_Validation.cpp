#include "LuaApi.h"

#include "LuaApiCommon.h"
#include "LuaRuntime/LuaStackGuard.h"

#include "PlaybackService.h"
#include "RecordPlayer.h"
#include "ScriptContext.h"
#include "TASEngine.h"
#include "TASProject.h"
#include "ValidationService.h"

#include <filesystem>

using namespace LuaApiCommon;

static ValidationService *RequireService(lua_State *L, const char *where) {
    ScriptContext *context = GetContext(L);
    ValidationService *service = context ? context->GetValidationService() : nullptr;
    if (!service) {
        Raise(L, where, "the validation service is not available");
    }
    return service;
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

// tas.validation.start([reference_path]) -> true | false, reason
//   Begins capturing a validation recording during active playback. When a
//   reference trace (a previous validation dump with physics) is supplied, the
//   subsequent stop() compares the live run against it frame-by-frame.
static int Start(lua_State *L) {
    const char *reference = luaL_optstring(L, 1, "");
    ValidationService *service = RequireService(L, "tas.validation.start");
    PlaybackService *playback = GetPlayback(L);
    RecordPlayer *recordPlayer = GetRecordPlayer(L);
    if (!(playback && playback->IsPlaying()) && !(recordPlayer && recordPlayer->IsPlaying())) {
        return PushFail(L, "validation recording requires an active playback");
    }
    const std::string outputPath = ResolveOutputPath(GetContext(L));
    const auto result = service->Start(outputPath, reference);
    return result.IsOk() ? PushOk(L) : PushFail(L, result.GetError().message);
}

// tas.validation.stop() -> report table | nil, reason
//   Stops the validation recording and writes the dump (plus a comparison report
//   when a reference was given to start()).
static int Stop(lua_State *L) {
    ValidationService *service = RequireService(L, "tas.validation.stop");
    const auto result = service->Stop();
    if (result.IsError()) {
        return PushNilReason(L, result.GetError().message);
    }
    PushReportTable(L, service->GetLastReport());
    return 1;
}

// tas.validation.get_report() -> report table of the most recent stop()
static int GetReport(lua_State *L) {
    PushReportTable(L, RequireService(L, "tas.validation.get_report")->GetLastReport());
    return 1;
}

static int IsActive(lua_State *L) {
    lua_pushboolean(L, RequireService(L, "tas.validation.is_active")->IsActive());
    return 1;
}

void LuaApi::RegisterValidationApi(lua_State *state, ScriptContext *context) {
    tas::lua::LuaStackGuard guard(state);
    lua_getglobal(state, "tas");
    lua_newtable(state);
    SetClosure(state, "start", Start, context);
    SetClosure(state, "stop", Stop, context);
    SetClosure(state, "get_report", GetReport, context);
    SetClosure(state, "is_active", IsActive, context);
    lua_setfield(state, -2, "validation");
    lua_pop(state, 1);
}
