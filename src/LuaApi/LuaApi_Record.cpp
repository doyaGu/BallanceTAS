#include "LuaApi.h"

#include "RecordPlayer.h"
#include "ScriptContext.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

static ScriptContext *GetContext(lua_State *L) {
    return static_cast<ScriptContext *>(lua_touserdata(L, lua_upvalueindex(1)));
}

static RecordPlayer *RequireRecordPlayer(lua_State *L, const char *functionName) {
    auto *context = GetContext(L);
    auto *recordPlayer = context ? context->GetRecordPlayer() : nullptr;
    if (!recordPlayer) {
        luaL_error(L, "%s: RecordPlayer not available", functionName);
    }
    return recordPlayer;
}

static size_t CheckFrame(lua_State *L, int index, const char *name) {
    const lua_Integer value = luaL_checkinteger(L, index);
    if (value < 0) {
        luaL_error(L, "%s must be non-negative", name);
    }
    return static_cast<size_t>(value);
}

static void PushString(lua_State *L, const std::string &value) {
    lua_pushlstring(L, value.data(), value.size());
}

static int Load(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    if (!path || !*path) {
        return luaL_error(L, "record.load: path cannot be empty");
    }
    lua_pushboolean(L, RequireRecordPlayer(L, "record.load")->LoadAndPlay(path));
    return 1;
}

static int Stop(lua_State *L) {
    RequireRecordPlayer(L, "record.stop")->Stop();
    return 0;
}

static int Pause(lua_State *L) {
    RequireRecordPlayer(L, "record.pause")->Pause();
    return 0;
}

static int Resume(lua_State *L) {
    RequireRecordPlayer(L, "record.resume")->Resume();
    return 0;
}

static int Seek(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.seek")->Seek(CheckFrame(L, 1, "frame")));
    return 1;
}

static int IsPlaying(lua_State *L) {
    auto *context = GetContext(L);
    auto *recordPlayer = context ? context->GetRecordPlayer() : nullptr;
    lua_pushboolean(L, recordPlayer && recordPlayer->IsPlaying());
    return 1;
}

static int IsPaused(lua_State *L) {
    auto *context = GetContext(L);
    auto *recordPlayer = context ? context->GetRecordPlayer() : nullptr;
    lua_pushboolean(L, recordPlayer && recordPlayer->IsPaused());
    return 1;
}

static int GetCurrentFrame(lua_State *L) {
    auto *context = GetContext(L);
    auto *recordPlayer = context ? context->GetRecordPlayer() : nullptr;
    lua_pushinteger(L, recordPlayer ? static_cast<lua_Integer>(recordPlayer->GetCurrentFrame()) : 0);
    return 1;
}

static int GetTotalFrames(lua_State *L) {
    auto *context = GetContext(L);
    auto *recordPlayer = context ? context->GetRecordPlayer() : nullptr;
    lua_pushinteger(L, recordPlayer ? static_cast<lua_Integer>(recordPlayer->GetTotalFrames()) : 0);
    return 1;
}

static void PushFrameInput(lua_State *L, const RecordFrameData &frameData) {
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

static int GetFrameInput(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.get_frame_input");
    RecordFrameData data;
    if (!recordPlayer->GetFrameInput(CheckFrame(L, 1, "frame"), &data)) {
        lua_pushnil(L);
        return 1;
    }
    PushFrameInput(L, data);
    return 1;
}

static int SetFrameKey(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.set_frame_key");
    const size_t frame = CheckFrame(L, 1, "frame");
    const char *key = luaL_checkstring(L, 2);
    const bool pressed = lua_toboolean(L, 3) != 0;
    lua_pushboolean(L, recordPlayer->SetFrameKey(frame, key ? key : "", pressed));
    return 1;
}

static int GetFrameDeltaTime(lua_State *L) {
    lua_pushnumber(L, RequireRecordPlayer(L, "record.get_frame_delta_time")->GetFrameDeltaTimeByFrame(CheckFrame(L, 1, "frame")));
    return 1;
}

static int SetFrameDeltaTime(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.set_frame_delta_time");
    lua_pushboolean(L, recordPlayer->SetFrameDeltaTime(CheckFrame(L, 1, "frame"), static_cast<float>(luaL_checknumber(L, 2))));
    return 1;
}

static int InsertFrames(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.insert_frames")->InsertFrames(CheckFrame(L, 1, "start_frame"), CheckFrame(L, 2, "count")));
    return 1;
}

static int DeleteFrames(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.delete_frames")->DeleteFrames(CheckFrame(L, 1, "start_frame"), CheckFrame(L, 2, "count")));
    return 1;
}

static int SeekRelative(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.seek_relative")->SeekRelative(static_cast<int>(luaL_checkinteger(L, 1))));
    return 1;
}

static int GetProgress(lua_State *L) {
    lua_pushnumber(L, RequireRecordPlayer(L, "record.get_progress")->GetProgress());
    return 1;
}

static int GetInputString(lua_State *L) {
    PushString(L, RequireRecordPlayer(L, "record.get_input_string")->GetInputString(CheckFrame(L, 1, "frame")));
    return 1;
}

static int Validate(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.validate")->Validate());
    return 1;
}

static int Save(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    if (!path || !*path) {
        return luaL_error(L, "record.save: path cannot be empty");
    }
    lua_pushboolean(L, RequireRecordPlayer(L, "record.save")->Save(path));
    return 1;
}

static int ExportInputs(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    const char *format = luaL_optstring(L, 2, "txt");
    lua_pushboolean(L, RequireRecordPlayer(L, "record.export_inputs")->ExportInputs(path ? path : "", format ? format : ""));
    return 1;
}

static int CopyFrames(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.copy_frames");
    lua_pushboolean(L, recordPlayer->CopyFrames(CheckFrame(L, 1, "source_frame"), CheckFrame(L, 2, "target_frame"),
                                                CheckFrame(L, 3, "count")));
    return 1;
}

static int DuplicateFrame(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.duplicate_frame");
    const size_t frame = CheckFrame(L, 1, "frame");
    const size_t count = lua_isnoneornil(L, 2) ? 1 : CheckFrame(L, 2, "count");
    lua_pushboolean(L, recordPlayer->DuplicateFrame(frame, count));
    return 1;
}

static int IsAtEnd(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.is_at_end")->IsAtEnd());
    return 1;
}

// --- Search & analysis ---

static void PushFrameList(lua_State *L, const std::vector<size_t> &frames) {
    lua_createtable(L, static_cast<int>(frames.size()), 0);
    for (size_t i = 0; i < frames.size(); ++i) {
        lua_pushinteger(L, static_cast<lua_Integer>(frames[i]));
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
}

static void PushStringList(lua_State *L, const std::vector<std::string> &values) {
    lua_createtable(L, static_cast<int>(values.size()), 0);
    for (size_t i = 0; i < values.size(); ++i) {
        PushString(L, values[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
}

static size_t OptEndFrame(lua_State *L, int index) {
    return lua_isnoneornil(L, index) ? static_cast<size_t>(-1) : CheckFrame(L, index, "end_frame");
}

static int GetPressedKeys(lua_State *L) {
    PushStringList(L, RequireRecordPlayer(L, "record.get_pressed_keys")->GetPressedKeys(CheckFrame(L, 1, "frame")));
    return 1;
}

static int FindInputChange(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.find_input_change");
    const int frame = recordPlayer->FindInputChange(CheckFrame(L, 1, "start_frame"), luaL_checkstring(L, 2));
    if (frame < 0) {
        lua_pushnil(L);
    } else {
        lua_pushinteger(L, frame);
    }
    return 1;
}

static int CompareFrames(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.compare_frames");
    lua_pushboolean(L, recordPlayer->CompareFrames(CheckFrame(L, 1, "frame1"), CheckFrame(L, 2, "frame2")));
    return 1;
}

static int FindFramesWithKeys(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.find_frames_with_keys");
    const char *keys = luaL_checkstring(L, 1);
    const size_t start = lua_isnoneornil(L, 2) ? 0 : CheckFrame(L, 2, "start_frame");
    PushFrameList(L, recordPlayer->FindFramesWithKeys(keys, start, OptEndFrame(L, 3)));
    return 1;
}

static int FindFramesByDeltaTime(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.find_frames_by_delta_time");
    const auto deltaTime = static_cast<float>(luaL_checknumber(L, 1));
    const auto tolerance = static_cast<float>(luaL_optnumber(L, 2, 0.01));
    PushFrameList(L, recordPlayer->FindFramesByDeltaTime(deltaTime, tolerance));
    return 1;
}

static int GetStatistics(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.get_statistics");
    const size_t start = lua_isnoneornil(L, 1) ? 0 : CheckFrame(L, 1, "start_frame");
    const auto stats = recordPlayer->GetStatistics(start, OptEndFrame(L, 2));
    lua_createtable(L, 0, static_cast<int>(stats.size()));
    for (const auto &[key, value] : stats) {
        lua_pushnumber(L, value);
        lua_setfield(L, -2, key.c_str());
    }
    return 1;
}

// --- Sections ---

static void PushSection(lua_State *L, const RecordSection *section) {
    if (!section) {
        lua_pushnil(L);
        return;
    }
    lua_createtable(L, 0, 5);
    PushString(L, section->name);
    lua_setfield(L, -2, "name");
    PushString(L, section->description);
    lua_setfield(L, -2, "description");
    lua_pushinteger(L, static_cast<lua_Integer>(section->startFrame));
    lua_setfield(L, -2, "start_frame");
    lua_pushinteger(L, static_cast<lua_Integer>(section->endFrame));
    lua_setfield(L, -2, "end_frame");
    PushString(L, section->color);
    lua_setfield(L, -2, "color");
}

static int AddSection(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.add_section");
    const size_t start = CheckFrame(L, 1, "start_frame");
    const size_t end = CheckFrame(L, 2, "end_frame");
    const char *name = luaL_checkstring(L, 3);
    lua_pushboolean(L, recordPlayer->AddSection(start, end, name, luaL_optstring(L, 4, ""), luaL_optstring(L, 5, "")));
    return 1;
}

static int RemoveSection(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.remove_section")->RemoveSection(luaL_checkstring(L, 1)));
    return 1;
}

static int GetSection(lua_State *L) {
    PushSection(L, RequireRecordPlayer(L, "record.get_section")->GetSection(luaL_checkstring(L, 1)));
    return 1;
}

static int GetSections(lua_State *L) {
    auto sections = RequireRecordPlayer(L, "record.get_sections")->GetAllSections();
    std::sort(sections.begin(), sections.end(), [](const RecordSection &a, const RecordSection &b) {
        return a.startFrame < b.startFrame;
    });
    lua_createtable(L, static_cast<int>(sections.size()), 0);
    for (size_t i = 0; i < sections.size(); ++i) {
        PushSection(L, &sections[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

static int GetSectionAtFrame(lua_State *L) {
    PushSection(L, RequireRecordPlayer(L, "record.get_section_at_frame")->GetSectionAtFrame(CheckFrame(L, 1, "frame")));
    return 1;
}

static int RenameSection(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.rename_section");
    lua_pushboolean(L, recordPlayer->RenameSection(luaL_checkstring(L, 1), luaL_checkstring(L, 2)));
    return 1;
}

static int MoveSection(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.move_section");
    lua_pushboolean(L, recordPlayer->MoveSection(luaL_checkstring(L, 1), CheckFrame(L, 2, "start_frame")));
    return 1;
}

static int ResizeSection(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.resize_section");
    lua_pushboolean(L, recordPlayer->ResizeSection(luaL_checkstring(L, 1), CheckFrame(L, 2, "start_frame"),
                                                   CheckFrame(L, 3, "end_frame")));
    return 1;
}

// --- Markers ---

static MarkerType CheckMarkerType(lua_State *L, int index) {
    if (lua_isnoneornil(L, index)) {
        return MarkerType::Bookmark;
    }
    const lua_Integer value = luaL_checkinteger(L, index);
    if (value < static_cast<lua_Integer>(MarkerType::Bookmark) || value > static_cast<lua_Integer>(MarkerType::Custom)) {
        luaL_error(L, "marker type must be one of tas.record.MarkerType");
    }
    return static_cast<MarkerType>(value);
}

// nil means "any type"
static std::optional<MarkerType> OptMarkerFilter(lua_State *L, int index) {
    if (lua_isnoneornil(L, index)) {
        return std::nullopt;
    }
    return CheckMarkerType(L, index);
}

static void PushMarker(lua_State *L, const RecordMarker *marker) {
    if (!marker) {
        lua_pushnil(L);
        return;
    }
    lua_createtable(L, 0, 5);
    PushString(L, marker->name);
    lua_setfield(L, -2, "name");
    lua_pushinteger(L, static_cast<lua_Integer>(marker->frame));
    lua_setfield(L, -2, "frame");
    lua_pushinteger(L, static_cast<lua_Integer>(marker->type));
    lua_setfield(L, -2, "type");
    PushString(L, marker->description);
    lua_setfield(L, -2, "description");
    PushString(L, marker->color);
    lua_setfield(L, -2, "color");
}

static void PushMarkerList(lua_State *L, std::vector<RecordMarker> markers) {
    std::sort(markers.begin(), markers.end(), [](const RecordMarker &a, const RecordMarker &b) {
        return a.frame != b.frame ? a.frame < b.frame : a.name < b.name;
    });
    lua_createtable(L, static_cast<int>(markers.size()), 0);
    for (size_t i = 0; i < markers.size(); ++i) {
        PushMarker(L, &markers[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
}

static int AddMarker(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.add_marker");
    const size_t frame = CheckFrame(L, 1, "frame");
    const char *name = luaL_checkstring(L, 2);
    const MarkerType type = CheckMarkerType(L, 3);
    lua_pushboolean(L, recordPlayer->AddMarker(frame, name, type, luaL_optstring(L, 4, ""), luaL_optstring(L, 5, "")));
    return 1;
}

static int RemoveMarker(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.remove_marker")->RemoveMarker(luaL_checkstring(L, 1)));
    return 1;
}

static int GetMarker(lua_State *L) {
    PushMarker(L, RequireRecordPlayer(L, "record.get_marker")->GetMarker(luaL_checkstring(L, 1)));
    return 1;
}

static int GetMarkers(lua_State *L) {
    PushMarkerList(L, RequireRecordPlayer(L, "record.get_markers")->GetAllMarkers());
    return 1;
}

static int GetMarkersAtFrame(lua_State *L) {
    PushMarkerList(L, RequireRecordPlayer(L, "record.get_markers_at_frame")->GetMarkersAtFrame(CheckFrame(L, 1, "frame")));
    return 1;
}

static int GetMarkersInRange(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.get_markers_in_range");
    PushMarkerList(L, recordPlayer->GetMarkersInRange(CheckFrame(L, 1, "start_frame"), CheckFrame(L, 2, "end_frame")));
    return 1;
}

static int RenameMarker(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.rename_marker");
    lua_pushboolean(L, recordPlayer->RenameMarker(luaL_checkstring(L, 1), luaL_checkstring(L, 2)));
    return 1;
}

static int MoveMarker(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.move_marker");
    lua_pushboolean(L, recordPlayer->MoveMarker(luaL_checkstring(L, 1), CheckFrame(L, 2, "frame")));
    return 1;
}

static int SeekToMarker(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.seek_to_marker")->SeekToMarker(luaL_checkstring(L, 1)));
    return 1;
}

static int GetNextMarker(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.get_next_marker");
    const size_t frame = lua_isnoneornil(L, 1) ? recordPlayer->GetCurrentFrame() : CheckFrame(L, 1, "frame");
    PushMarker(L, recordPlayer->GetNextMarker(frame, OptMarkerFilter(L, 2)));
    return 1;
}

static int GetPreviousMarker(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.get_previous_marker");
    const size_t frame = lua_isnoneornil(L, 1) ? recordPlayer->GetCurrentFrame() : CheckFrame(L, 1, "frame");
    PushMarker(L, recordPlayer->GetPreviousMarker(frame, OptMarkerFilter(L, 2)));
    return 1;
}

// --- Comments (identified by the stable id add_comment returns) ---

static void PushComment(lua_State *L, const RecordComment *comment) {
    if (!comment) {
        lua_pushnil(L);
        return;
    }
    lua_createtable(L, 0, 7);
    lua_pushinteger(L, static_cast<lua_Integer>(comment->id));
    lua_setfield(L, -2, "id");
    PushString(L, comment->text);
    lua_setfield(L, -2, "text");
    lua_pushinteger(L, static_cast<lua_Integer>(comment->startFrame));
    lua_setfield(L, -2, "start_frame");
    lua_pushinteger(L, static_cast<lua_Integer>(comment->endFrame));
    lua_setfield(L, -2, "end_frame");
    PushString(L, comment->author);
    lua_setfield(L, -2, "author");
    PushString(L, comment->category);
    lua_setfield(L, -2, "category");
    lua_pushinteger(L, static_cast<lua_Integer>(comment->timestamp));
    lua_setfield(L, -2, "timestamp");
}

static int AddComment(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.add_comment");
    const size_t frame = CheckFrame(L, 1, "frame");
    const char *text = luaL_checkstring(L, 2);
    lua_pushinteger(L, static_cast<lua_Integer>(
        recordPlayer->AddComment(frame, text, luaL_optstring(L, 3, ""), luaL_optstring(L, 4, ""))));
    return 1;
}

static int AddRangeComment(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.add_range_comment");
    const size_t start = CheckFrame(L, 1, "start_frame");
    const size_t end = CheckFrame(L, 2, "end_frame");
    if (end < start) {
        return luaL_error(L, "record.add_range_comment: end_frame must not be before start_frame");
    }
    const char *text = luaL_checkstring(L, 3);
    lua_pushinteger(L, static_cast<lua_Integer>(
        recordPlayer->AddRangeComment(start, end, text, luaL_optstring(L, 4, ""), luaL_optstring(L, 5, ""))));
    return 1;
}

static int RemoveComment(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.remove_comment")->RemoveComment(CheckFrame(L, 1, "id")));
    return 1;
}

static int GetComment(lua_State *L) {
    PushComment(L, RequireRecordPlayer(L, "record.get_comment")->GetComment(CheckFrame(L, 1, "id")));
    return 1;
}

static int EditComment(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.edit_comment");
    lua_pushboolean(L, recordPlayer->EditComment(CheckFrame(L, 1, "id"), luaL_checkstring(L, 2)));
    return 1;
}

static int GetAllComments(lua_State *L) {
    const auto comments = RequireRecordPlayer(L, "record.get_all_comments")->GetAllComments();
    lua_createtable(L, static_cast<int>(comments.size()), 0);
    for (size_t i = 0; i < comments.size(); ++i) {
        PushComment(L, &comments[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

static int GetCommentsAtFrame(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.get_comments_at_frame");
    const size_t frame = CheckFrame(L, 1, "frame");
    const auto comments = recordPlayer->GetAllComments();
    lua_newtable(L);
    lua_Integer slot = 1;
    for (size_t i = 0; i < comments.size(); ++i) {
        if (frame >= comments[i].startFrame && frame <= comments[i].endFrame) {
            PushComment(L, &comments[i]);
            lua_rawseti(L, -2, slot++);
        }
    }
    return 1;
}

// --- Macros ---

static void PushMacro(lua_State *L, const RecordMacro *macro) {
    if (!macro) {
        lua_pushnil(L);
        return;
    }
    lua_createtable(L, 0, 3);
    PushString(L, macro->name);
    lua_setfield(L, -2, "name");
    PushString(L, macro->description);
    lua_setfield(L, -2, "description");
    lua_pushinteger(L, static_cast<lua_Integer>(macro->frames.size()));
    lua_setfield(L, -2, "frame_count");
}

static int SaveMacro(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.save_macro");
    const char *name = luaL_checkstring(L, 1);
    const size_t start = CheckFrame(L, 2, "start_frame");
    const size_t end = CheckFrame(L, 3, "end_frame");
    lua_pushboolean(L, recordPlayer->SaveMacro(name, start, end, luaL_optstring(L, 4, "")));
    return 1;
}

static int DeleteMacro(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.delete_macro")->DeleteMacro(luaL_checkstring(L, 1)));
    return 1;
}

static int GetMacro(lua_State *L) {
    PushMacro(L, RequireRecordPlayer(L, "record.get_macro")->GetMacro(luaL_checkstring(L, 1)));
    return 1;
}

static int GetMacros(lua_State *L) {
    auto macros = RequireRecordPlayer(L, "record.get_macros")->GetAllMacros();
    std::sort(macros.begin(), macros.end(), [](const RecordMacro &a, const RecordMacro &b) { return a.name < b.name; });
    lua_createtable(L, static_cast<int>(macros.size()), 0);
    for (size_t i = 0; i < macros.size(); ++i) {
        PushMacro(L, &macros[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

static int InsertMacro(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.insert_macro");
    const char *name = luaL_checkstring(L, 1);
    const size_t frame = CheckFrame(L, 2, "frame");
    const size_t repeat = lua_isnoneornil(L, 3) ? 1 : CheckFrame(L, 3, "repeat_count");
    lua_pushboolean(L, recordPlayer->InsertMacro(name, frame, repeat));
    return 1;
}

// --- Metadata & tags ---

static int SetMetadata(lua_State *L) {
    RequireRecordPlayer(L, "record.set_metadata")->SetMetadataField(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
    return 0;
}

static int GetMetadata(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.get_metadata");
    if (!lua_isnoneornil(L, 1)) {
        PushString(L, recordPlayer->GetMetadataField(luaL_checkstring(L, 1)));
        return 1;
    }
    const RecordMetadata &metadata = recordPlayer->GetMetadata();
    lua_newtable(L);
    PushString(L, metadata.title);
    lua_setfield(L, -2, "title");
    PushString(L, metadata.author);
    lua_setfield(L, -2, "author");
    PushString(L, metadata.description);
    lua_setfield(L, -2, "description");
    PushString(L, metadata.gameVersion);
    lua_setfield(L, -2, "game_version");
    PushString(L, metadata.tasVersion);
    lua_setfield(L, -2, "tas_version");
    lua_pushinteger(L, static_cast<lua_Integer>(metadata.rerecordCount));
    lua_setfield(L, -2, "rerecord_count");
    PushStringList(L, metadata.tags);
    lua_setfield(L, -2, "tags");
    // Custom fields live in their own table so they cannot shadow the built-in ones.
    lua_createtable(L, 0, static_cast<int>(metadata.customFields.size()));
    for (const auto &[key, value] : metadata.customFields) {
        PushString(L, value);
        lua_setfield(L, -2, key.c_str());
    }
    lua_setfield(L, -2, "custom");
    return 1;
}

static int AddTag(lua_State *L) {
    RequireRecordPlayer(L, "record.add_tag")->AddTag(luaL_checkstring(L, 1));
    return 0;
}

static int RemoveTag(lua_State *L) {
    RequireRecordPlayer(L, "record.remove_tag")->RemoveTag(luaL_checkstring(L, 1));
    return 0;
}

static int GetTags(lua_State *L) {
    PushStringList(L, RequireRecordPlayer(L, "record.get_tags")->GetTags());
    return 1;
}

static int IncrementRerecordCount(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.increment_rerecord_count");
    recordPlayer->IncrementRerecordCount();
    lua_pushinteger(L, static_cast<lua_Integer>(recordPlayer->GetRerecordCount()));
    return 1;
}

static int GetRerecordCount(lua_State *L) {
    lua_pushinteger(L, static_cast<lua_Integer>(RequireRecordPlayer(L, "record.get_rerecord_count")->GetRerecordCount()));
    return 1;
}

// --- Undo / redo ---

static int Undo(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.undo")->Undo());
    return 1;
}

static int Redo(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.redo")->Redo());
    return 1;
}

static int CanUndo(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.can_undo")->CanUndo());
    return 1;
}

static int CanRedo(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.can_redo")->CanRedo());
    return 1;
}

static int GetUndoDescription(lua_State *L) {
    PushString(L, RequireRecordPlayer(L, "record.get_undo_description")->GetUndoDescription());
    return 1;
}

static int GetRedoDescription(lua_State *L) {
    PushString(L, RequireRecordPlayer(L, "record.get_redo_description")->GetRedoDescription());
    return 1;
}

static int ClearHistory(lua_State *L) {
    RequireRecordPlayer(L, "record.clear_history")->ClearHistory();
    return 0;
}

static int GetHistorySize(lua_State *L) {
    lua_pushinteger(L, static_cast<lua_Integer>(RequireRecordPlayer(L, "record.get_history_size")->GetHistorySize()));
    return 1;
}

static int SetMaxHistorySize(lua_State *L) {
    RequireRecordPlayer(L, "record.set_max_history_size")->SetMaxHistorySize(CheckFrame(L, 1, "size"));
    return 0;
}

// --- Branches ---

static int CreateBranch(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.create_branch");
    const char *name = luaL_checkstring(L, 1);
    const size_t frame = CheckFrame(L, 2, "divergence_frame");
    lua_pushboolean(L, recordPlayer->CreateBranch(name, frame, luaL_optstring(L, 3, "")));
    return 1;
}

static int DeleteBranch(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.delete_branch")->DeleteBranch(luaL_checkstring(L, 1)));
    return 1;
}

static int SwitchBranch(lua_State *L) {
    const char *name = luaL_optstring(L, 1, "");
    lua_pushboolean(L, RequireRecordPlayer(L, "record.switch_branch")->SwitchBranch(name));
    return 1;
}

static int GetCurrentBranch(lua_State *L) {
    PushString(L, RequireRecordPlayer(L, "record.get_current_branch")->GetCurrentBranch());
    return 1;
}

static int GetBranches(lua_State *L) {
    auto branches = RequireRecordPlayer(L, "record.get_branches")->GetAllBranches();
    std::sort(branches.begin(), branches.end(), [](const RecordBranch &a, const RecordBranch &b) { return a.name < b.name; });
    lua_createtable(L, static_cast<int>(branches.size()), 0);
    for (size_t i = 0; i < branches.size(); ++i) {
        const RecordBranch &branch = branches[i];
        lua_createtable(L, 0, 5);
        PushString(L, branch.name);
        lua_setfield(L, -2, "name");
        PushString(L, branch.description);
        lua_setfield(L, -2, "description");
        lua_pushinteger(L, static_cast<lua_Integer>(branch.divergenceFrame));
        lua_setfield(L, -2, "divergence_frame");
        PushString(L, branch.parentBranch);
        lua_setfield(L, -2, "parent");
        lua_pushinteger(L, static_cast<lua_Integer>(branch.frames.size()));
        lua_setfield(L, -2, "frame_count");
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

// --- Savestate links ---

static void PushSavestateLink(lua_State *L, const SavestateLink *link) {
    if (!link) {
        lua_pushnil(L);
        return;
    }
    lua_createtable(L, 0, 4);
    lua_pushinteger(L, static_cast<lua_Integer>(link->frame));
    lua_setfield(L, -2, "frame");
    PushString(L, link->savestatePath);
    lua_setfield(L, -2, "path");
    PushString(L, link->description);
    lua_setfield(L, -2, "description");
    lua_pushinteger(L, static_cast<lua_Integer>(link->createdAt));
    lua_setfield(L, -2, "created_at");
}

static int LinkSavestate(lua_State *L) {
    auto *recordPlayer = RequireRecordPlayer(L, "record.link_savestate");
    const size_t frame = CheckFrame(L, 1, "frame");
    const char *path = luaL_checkstring(L, 2);
    lua_pushboolean(L, recordPlayer->LinkSavestate(frame, path, luaL_optstring(L, 3, "")));
    return 1;
}

static int UnlinkSavestate(lua_State *L) {
    lua_pushboolean(L, RequireRecordPlayer(L, "record.unlink_savestate")->UnlinkSavestate(CheckFrame(L, 1, "frame")));
    return 1;
}

static int GetSavestateLink(lua_State *L) {
    PushSavestateLink(L, RequireRecordPlayer(L, "record.get_savestate_link")->GetSavestateLink(CheckFrame(L, 1, "frame")));
    return 1;
}

static int GetAllSavestateLinks(lua_State *L) {
    auto links = RequireRecordPlayer(L, "record.get_all_savestate_links")->GetAllSavestateLinks();
    std::sort(links.begin(), links.end(), [](const SavestateLink &a, const SavestateLink &b) { return a.frame < b.frame; });
    lua_createtable(L, static_cast<int>(links.size()), 0);
    for (size_t i = 0; i < links.size(); ++i) {
        PushSavestateLink(L, &links[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

static void SetRecordFunction(lua_State *L, const char *name, lua_CFunction function, ScriptContext *context) {
    lua_pushlightuserdata(L, context);
    lua_pushcclosure(L, function, 1);
    lua_setfield(L, -2, name);
}

static void RegisterMarkerType(lua_State *L) {
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
    SetRecordFunction(state, "copy_frames", CopyFrames, context);
    SetRecordFunction(state, "duplicate_frame", DuplicateFrame, context);
    SetRecordFunction(state, "is_at_end", IsAtEnd, context);
    SetRecordFunction(state, "get_pressed_keys", GetPressedKeys, context);
    SetRecordFunction(state, "find_input_change", FindInputChange, context);
    SetRecordFunction(state, "compare_frames", CompareFrames, context);
    SetRecordFunction(state, "find_frames_with_keys", FindFramesWithKeys, context);
    SetRecordFunction(state, "find_frames_by_delta_time", FindFramesByDeltaTime, context);
    SetRecordFunction(state, "get_statistics", GetStatistics, context);
    SetRecordFunction(state, "add_section", AddSection, context);
    SetRecordFunction(state, "remove_section", RemoveSection, context);
    SetRecordFunction(state, "get_section", GetSection, context);
    SetRecordFunction(state, "get_sections", GetSections, context);
    SetRecordFunction(state, "get_section_at_frame", GetSectionAtFrame, context);
    SetRecordFunction(state, "rename_section", RenameSection, context);
    SetRecordFunction(state, "move_section", MoveSection, context);
    SetRecordFunction(state, "resize_section", ResizeSection, context);
    SetRecordFunction(state, "add_marker", AddMarker, context);
    SetRecordFunction(state, "remove_marker", RemoveMarker, context);
    SetRecordFunction(state, "get_marker", GetMarker, context);
    SetRecordFunction(state, "get_markers", GetMarkers, context);
    SetRecordFunction(state, "get_markers_at_frame", GetMarkersAtFrame, context);
    SetRecordFunction(state, "get_markers_in_range", GetMarkersInRange, context);
    SetRecordFunction(state, "rename_marker", RenameMarker, context);
    SetRecordFunction(state, "move_marker", MoveMarker, context);
    SetRecordFunction(state, "seek_to_marker", SeekToMarker, context);
    SetRecordFunction(state, "get_next_marker", GetNextMarker, context);
    SetRecordFunction(state, "get_previous_marker", GetPreviousMarker, context);
    SetRecordFunction(state, "add_comment", AddComment, context);
    SetRecordFunction(state, "add_range_comment", AddRangeComment, context);
    SetRecordFunction(state, "remove_comment", RemoveComment, context);
    SetRecordFunction(state, "get_comment", GetComment, context);
    SetRecordFunction(state, "edit_comment", EditComment, context);
    SetRecordFunction(state, "get_all_comments", GetAllComments, context);
    SetRecordFunction(state, "get_comments_at_frame", GetCommentsAtFrame, context);
    SetRecordFunction(state, "save_macro", SaveMacro, context);
    SetRecordFunction(state, "delete_macro", DeleteMacro, context);
    SetRecordFunction(state, "get_macro", GetMacro, context);
    SetRecordFunction(state, "get_macros", GetMacros, context);
    SetRecordFunction(state, "insert_macro", InsertMacro, context);
    SetRecordFunction(state, "set_metadata", SetMetadata, context);
    SetRecordFunction(state, "get_metadata", GetMetadata, context);
    SetRecordFunction(state, "add_tag", AddTag, context);
    SetRecordFunction(state, "remove_tag", RemoveTag, context);
    SetRecordFunction(state, "get_tags", GetTags, context);
    SetRecordFunction(state, "increment_rerecord_count", IncrementRerecordCount, context);
    SetRecordFunction(state, "get_rerecord_count", GetRerecordCount, context);
    SetRecordFunction(state, "undo", Undo, context);
    SetRecordFunction(state, "redo", Redo, context);
    SetRecordFunction(state, "can_undo", CanUndo, context);
    SetRecordFunction(state, "can_redo", CanRedo, context);
    SetRecordFunction(state, "get_undo_description", GetUndoDescription, context);
    SetRecordFunction(state, "get_redo_description", GetRedoDescription, context);
    SetRecordFunction(state, "clear_history", ClearHistory, context);
    SetRecordFunction(state, "get_history_size", GetHistorySize, context);
    SetRecordFunction(state, "set_max_history_size", SetMaxHistorySize, context);
    SetRecordFunction(state, "create_branch", CreateBranch, context);
    SetRecordFunction(state, "delete_branch", DeleteBranch, context);
    SetRecordFunction(state, "switch_branch", SwitchBranch, context);
    SetRecordFunction(state, "get_current_branch", GetCurrentBranch, context);
    SetRecordFunction(state, "get_branches", GetBranches, context);
    SetRecordFunction(state, "link_savestate", LinkSavestate, context);
    SetRecordFunction(state, "unlink_savestate", UnlinkSavestate, context);
    SetRecordFunction(state, "get_savestate_link", GetSavestateLink, context);
    SetRecordFunction(state, "get_all_savestate_links", GetAllSavestateLinks, context);
    lua_setfield(state, -2, "record");
    lua_pop(state, 1);
}
