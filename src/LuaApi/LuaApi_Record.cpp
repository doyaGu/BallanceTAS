#include "LuaApi.h"

#include "RecordPlayer.h"
#include "ScriptContext.h"

#include <string>

namespace {

ScriptContext *GetContext(lua_State *L) {
    return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

RecordPlayer *RequireRecordPlayer(lua_State *L, const char *functionName) {
    auto *context = GetContext(L);
    auto *recordPlayer = context ? context->GetRecordPlayer() : nullptr;
    if (!recordPlayer) {
        luaL_error(L, "%s: RecordPlayer not available", functionName);
    }
    return recordPlayer;
}

size_t CheckFrame(lua_State *L, int index, const char *name) {
    const lua_Integer value = luaL_checkinteger(L, index);
    if (value < 0) {
        luaL_error(L, "%s must be non-negative", name);
    }
    return static_cast<size_t>(value);
}

void PushString(lua_State *L, const std::string &value) {
    lua_pushlstring(L, value.data(), value.size());
}

int Load(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    if (!path || !*path) {
        return luaL_error(L, "record.load: path cannot be empty");
    }
    lua_pushboolean(L, RequireRecordPlayer(L, "record.load")->LoadAndPlay(path));
    return 1;
}

int Stop(lua_State *L) {
    RequireRecordPlayer(L, "record.stop")->Stop();
    return 0;
}

int Pause(lua_State *L) {
    RequireRecordPlayer(L, "record.pause")->Pause();
    return 0;
}

int Resume(lua_State *L) {
    RequireRecordPlayer(L, "record.resume")->Resume();
    return 0;
}

int Seek(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.seek")->Seek(CheckFrame(L, 1, "frame")));
    return 1;
}

int IsPlaying(lua_State *L) {
    auto *context = GetContext(L);
    auto *recordPlayer = context ? context->GetRecordPlayer() : nullptr;
    lua_pushboolean(L, recordPlayer && recordPlayer->IsPlaying());
    return 1;
}

int IsPaused(lua_State *L) {
    auto *context = GetContext(L);
    auto *recordPlayer = context ? context->GetRecordPlayer() : nullptr;
    lua_pushboolean(L, recordPlayer && recordPlayer->IsPaused());
    return 1;
}

int GetCurrentFrame(lua_State *L) {
    auto *context = GetContext(L);
    auto *recordPlayer = context ? context->GetRecordPlayer() : nullptr;
    lua_pushinteger(L, recordPlayer ? static_cast<lua_Integer>(recordPlayer->GetCurrentFrame()) : 0);
    return 1;
}

int GetTotalFrames(lua_State *L) {
    auto *context = GetContext(L);
    auto *recordPlayer = context ? context->GetRecordPlayer() : nullptr;
    lua_pushinteger(L, recordPlayer ? static_cast<lua_Integer>(recordPlayer->GetTotalFrames()) : 0);
    return 1;
}

void PushFrameInput(lua_State *L, const RecordFrameData &frameData) {
    lua_newtable(L);
    lua_pushnumber(L, frameData.deltaTime);
    lua_setfield(L, -2, "delta_time");

    lua_newtable(L);
    lua_pushboolean(L, frameData.keyState.key_up != 0);
    lua_setfield(L, -2, "up");
    lua_pushboolean(L, frameData.keyState.key_down != 0);
    lua_setfield(L, -2, "down");
    lua_pushboolean(L, frameData.keyState.key_left != 0);
    lua_setfield(L, -2, "left");
    lua_pushboolean(L, frameData.keyState.key_right != 0);
    lua_setfield(L, -2, "right");
    lua_pushboolean(L, frameData.keyState.key_shift != 0);
    lua_setfield(L, -2, "shift");
    lua_pushboolean(L, frameData.keyState.key_space != 0);
    lua_setfield(L, -2, "space");
    lua_setfield(L, -2, "keys");
}

int GetFrameInput(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.get_frame_input");
    RecordFrameData data;
    if (!recordPlayer->GetFrameInput(CheckFrame(L, 1, "frame"), &data)) {
        lua_pushnil(L);
        return 1;
    }
    PushFrameInput(L, data);
    return 1;
}

int SetFrameKey(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.set_frame_key");
    const size_t frame = CheckFrame(L, 1, "frame");
    const char *key = luaL_checkstring(L, 2);
    const bool pressed = lua_toboolean(L, 3) != 0;
    lua_pushboolean(L, recordPlayer->SetFrameKey(frame, key ? key : "", pressed));
    return 1;
}

int GetFrameDeltaTime(lua_State *L) {
    lua_pushnumber(L, RequireRecordPlayer(L, "record.get_frame_delta_time")->GetFrameDeltaTimeByFrame(CheckFrame(L, 1, "frame")));
    return 1;
}

int SetFrameDeltaTime(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.set_frame_delta_time");
    lua_pushboolean(L, recordPlayer->SetFrameDeltaTime(CheckFrame(L, 1, "frame"), static_cast<float>(luaL_checknumber(L, 2))));
    return 1;
}

int InsertFrames(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.insert_frames")->InsertFrames(CheckFrame(L, 1, "start_frame"), CheckFrame(L, 2, "count")));
    return 1;
}

int DeleteFrames(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.delete_frames")->DeleteFrames(CheckFrame(L, 1, "start_frame"), CheckFrame(L, 2, "count")));
    return 1;
}

int SeekRelative(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.seek_relative")->SeekRelative(static_cast<int>(luaL_checkinteger(L, 1))));
    return 1;
}

int GetProgress(lua_State *L) {
    lua_pushnumber(L, RequireRecordPlayer(L, "record.get_progress")->GetProgress());
    return 1;
}

int GetInputString(lua_State *L) {
    PushString(L, RequireRecordPlayer(L, "record.get_input_string")->GetInputString(CheckFrame(L, 1, "frame")));
    return 1;
}

int Validate(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.validate")->Validate());
    return 1;
}

int Save(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    if (!path || !*path) {
        return luaL_error(L, "record.save: path cannot be empty");
    }
    lua_pushboolean(L, RequireRecordPlayer(L, "record.save")->Save(path));
    return 1;
}

int ExportInputs(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    const char *format = luaL_optstring(L, 2, "txt");
    lua_pushboolean(L, RequireRecordPlayer(L, "record.export_inputs")->ExportInputs(path ? path : "", format ? format : ""));
    return 1;
}

void SetRecordFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, function, 1);
    lua_setfield(L, -2, name);
}

int UnsupportedRecordFunction(lua_State *L) {
    const char *name = lua_tostring(L, lua_upvalueindex(1));
    return luaL_error(L, "record.%s: compatibility entry exists but is not implemented in the Lua C API migration yet", name ? name : "<unknown>");
}

void SetUnsupportedRecordFunction(lua_State *L, const char *name) {
    lua_pushstring(L, name);
    lua_pushcclosure(L, UnsupportedRecordFunction, 1);
    lua_setfield(L, -2, name);
}

void RegisterMarkerType(lua_State *L) {
    lua_newtable(L);
    lua_pushinteger(L, static_cast<lua_Integer>(MarkerType::Bookmark));
    lua_setfield(L, -2, "BOOKMARK");
    lua_pushinteger(L, static_cast<lua_Integer>(MarkerType::Checkpoint));
    lua_setfield(L, -2, "CHECKPOINT");
    lua_pushinteger(L, static_cast<lua_Integer>(MarkerType::Bug));
    lua_setfield(L, -2, "BUG");
    lua_pushinteger(L, static_cast<lua_Integer>(MarkerType::Trick));
    lua_setfield(L, -2, "TRICK");
    lua_pushinteger(L, static_cast<lua_Integer>(MarkerType::Sync));
    lua_setfield(L, -2, "SYNC");
    lua_pushinteger(L, static_cast<lua_Integer>(MarkerType::Custom));
    lua_setfield(L, -2, "CUSTOM");
    lua_setfield(L, -2, "MarkerType");
}

} // namespace

void LuaApi::RegisterRecordApi(lua_State *state, ScriptContext *context) {
    if (!state || !context) {
        throw std::runtime_error("LuaApi::RegisterRecordApi requires a valid Lua state and ScriptContext");
    }

    lua_getglobal(state, "tas");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_pushvalue(state, -1);
        lua_setglobal(state, "tas");
    }

    lua_newtable(state);
    SetRecordFunction(state, "load", Load, context);
    SetRecordFunction(state, "stop", Stop, context);
    SetRecordFunction(state, "pause", Pause, context);
    SetRecordFunction(state, "resume", Resume, context);
    SetRecordFunction(state, "seek", Seek, context);
    SetRecordFunction(state, "is_playing", IsPlaying, context);
    SetRecordFunction(state, "is_paused", IsPaused, context);
    SetRecordFunction(state, "get_current_frame", GetCurrentFrame, context);
    SetRecordFunction(state, "get_total_frames", GetTotalFrames, context);
    SetRecordFunction(state, "get_frame_input", GetFrameInput, context);
    SetRecordFunction(state, "set_frame_key", SetFrameKey, context);
    SetRecordFunction(state, "get_frame_delta_time", GetFrameDeltaTime, context);
    SetRecordFunction(state, "set_frame_delta_time", SetFrameDeltaTime, context);
    SetRecordFunction(state, "insert_frames", InsertFrames, context);
    SetRecordFunction(state, "delete_frames", DeleteFrames, context);
    SetRecordFunction(state, "seek_relative", SeekRelative, context);
    SetRecordFunction(state, "get_progress", GetProgress, context);
    SetRecordFunction(state, "get_input_string", GetInputString, context);
    SetRecordFunction(state, "validate", Validate, context);
    SetRecordFunction(state, "save", Save, context);
    SetRecordFunction(state, "export_inputs", ExportInputs, context);
    RegisterMarkerType(state);
    constexpr const char *kUnsupportedRecordFunctions[] = {
        "add_comment", "add_marker", "add_range_comment", "add_section", "add_tag",
        "can_redo", "can_undo", "clear_history", "compare_frames", "copy_frames",
        "create_branch", "delete_branch", "delete_macro", "duplicate_frame", "edit_comment",
        "find_frames_by_delta_time", "find_frames_with_keys", "find_input_change",
        "get_all_comments", "get_all_savestate_links", "get_branches", "get_comment",
        "get_comments_at_frame", "get_current_branch", "get_history_size", "get_macro",
        "get_macros", "get_marker", "get_markers", "get_markers_at_frame",
        "get_markers_in_range", "get_metadata", "get_next_marker", "get_playback_speed",
        "get_pressed_keys", "get_previous_marker", "get_redo_description",
        "get_rerecord_count", "get_savestate_link", "get_section", "get_section_at_frame",
        "get_sections", "get_statistics", "get_tags", "get_undo_description",
        "increment_rerecord_count", "insert_macro", "is_at_end", "link_savestate",
        "move_marker", "move_section", "redo", "remove_comment", "remove_marker",
        "remove_section", "remove_tag", "rename_marker", "rename_section", "resize_section",
        "save_macro", "seek_to_marker", "set_max_history_size", "set_metadata",
        "set_playback_speed", "switch_branch", "undo", "unlink_savestate",
    };
    for (const char *name : kUnsupportedRecordFunctions) {
        SetUnsupportedRecordFunction(state, name);
    }
    lua_setfield(state, -2, "record");
    lua_pop(state, 1);
}
