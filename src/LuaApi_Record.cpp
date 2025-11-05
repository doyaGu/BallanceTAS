#include "LuaApi.h"

#include <stdexcept>

#include "TASEngine.h"
#include "ScriptContext.h"
#include "RecordPlayer.h"

// ===================================================================
// Record Playback & Editing API Registration
// ===================================================================

void LuaApi::RegisterRecordApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterRecordApi requires a valid ScriptContext");
    }

    auto *recordPlayer = context->GetRecordPlayer();

    // Create nested 'record' table
    sol::table record = tas["record"] = tas.create();

    // tas.record.load(path) - Load and start playing a record file
    record["load"] = [recordPlayer](const std::string &path) -> bool {
        if (path.empty()) {
            throw sol::error("record.load: path cannot be empty");
        }
        if (!recordPlayer) {
            throw sol::error("record.load: RecordPlayer not available");
        }
        return recordPlayer->LoadAndPlay(path);
    };

    // tas.record.stop() - Stop current playback
    record["stop"] = [recordPlayer]() {
        if (!recordPlayer) {
            throw sol::error("record.stop: RecordPlayer not available");
        }
        recordPlayer->Stop();
    };

    // tas.record.pause() - Pause current playback
    record["pause"] = [recordPlayer]() {
        if (!recordPlayer) {
            throw sol::error("record.pause: RecordPlayer not available");
        }
        recordPlayer->Pause();
    };

    // tas.record.resume() - Resume paused playback
    record["resume"] = [recordPlayer]() {
        if (!recordPlayer) {
            throw sol::error("record.resume: RecordPlayer not available");
        }
        recordPlayer->Resume();
    };

    // tas.record.seek(frame) - Seek to a specific frame
    record["seek"] = [recordPlayer](int frame) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.seek: RecordPlayer not available");
        }
        if (frame < 0) {
            throw sol::error("record.seek: frame must be non-negative");
        }
        return recordPlayer->Seek(static_cast<size_t>(frame));
    };

    // tas.record.is_playing() - Check if a record is currently playing
    record["is_playing"] = [recordPlayer]() -> bool {
        if (!recordPlayer) {
            return false;
        }
        return recordPlayer->IsPlaying();
    };

    // tas.record.is_paused() - Check if playback is paused
    record["is_paused"] = [recordPlayer]() -> bool {
        if (!recordPlayer) {
            return false;
        }
        return recordPlayer->IsPaused();
    };

    // tas.record.get_current_frame() - Get the current playback frame
    record["get_current_frame"] = [recordPlayer]() -> int {
        if (!recordPlayer) {
            return 0;
        }
        return static_cast<int>(recordPlayer->GetCurrentFrame());
    };

    // tas.record.get_total_frames() - Get the total number of frames
    record["get_total_frames"] = [recordPlayer]() -> int {
        if (!recordPlayer) {
            return 0;
        }
        return static_cast<int>(recordPlayer->GetTotalFrames());
    };

    // ===================================================================
    // Frame-Level Input Query & Modification APIs
    // ===================================================================

    // tas.record.get_frame_input(frame) - Get input data for a specific frame
    record["get_frame_input"] = [recordPlayer, context](int frame) -> sol::object {
        if (!recordPlayer) {
            throw sol::error("record.get_frame_input: RecordPlayer not available");
        }
        if (frame < 0) {
            throw sol::error("record.get_frame_input: frame must be non-negative");
        }

        RecordFrameData frameData;
        if (!recordPlayer->GetFrameInput(static_cast<size_t>(frame), &frameData)) {
            return sol::nil;
        }

        // Return as a Lua table
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();
        result["delta_time"] = frameData.deltaTime;

        sol::table keys = lua.create_table();
        keys["up"] = frameData.keyState.key_up != 0;
        keys["down"] = frameData.keyState.key_down != 0;
        keys["left"] = frameData.keyState.key_left != 0;
        keys["right"] = frameData.keyState.key_right != 0;
        keys["shift"] = frameData.keyState.key_shift != 0;
        keys["space"] = frameData.keyState.key_space != 0;
        keys["q"] = frameData.keyState.key_q != 0;
        keys["esc"] = frameData.keyState.key_esc != 0;
        keys["enter"] = frameData.keyState.key_enter != 0;
        result["keys"] = keys;

        return result;
    };

    // tas.record.set_frame_key(frame, key, pressed) - Set a specific key state for a frame
    record["set_frame_key"] = [recordPlayer](int frame, const std::string &key, bool pressed) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.set_frame_key: RecordPlayer not available");
        }
        if (frame < 0) {
            throw sol::error("record.set_frame_key: frame must be non-negative");
        }
        return recordPlayer->SetFrameKey(static_cast<size_t>(frame), key, pressed);
    };

    // tas.record.get_frame_delta_time(frame) - Get delta time for a specific frame
    record["get_frame_delta_time"] = [recordPlayer](int frame) -> float {
        if (!recordPlayer) {
            return 0.0f;
        }
        if (frame < 0) {
            return 0.0f;
        }
        return recordPlayer->GetFrameDeltaTimeByFrame(static_cast<size_t>(frame));
    };

    // tas.record.set_frame_delta_time(frame, delta_time) - Set delta time for a specific frame
    record["set_frame_delta_time"] = [recordPlayer](int frame, float deltaTime) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.set_frame_delta_time: RecordPlayer not available");
        }
        if (frame < 0) {
            throw sol::error("record.set_frame_delta_time: frame must be non-negative");
        }
        return recordPlayer->SetFrameDeltaTime(static_cast<size_t>(frame), deltaTime);
    };

    // ===================================================================
    // Bulk Frame Operations APIs
    // ===================================================================

    // tas.record.insert_frames(start_frame, count) - Insert blank frames
    record["insert_frames"] = [recordPlayer](int startFrame, int count) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.insert_frames: RecordPlayer not available");
        }
        if (startFrame < 0) {
            throw sol::error("record.insert_frames: start_frame must be non-negative");
        }
        if (count < 0) {
            throw sol::error("record.insert_frames: count must be non-negative");
        }
        return recordPlayer->InsertFrames(static_cast<size_t>(startFrame), static_cast<size_t>(count));
    };

    // tas.record.delete_frames(start_frame, count) - Delete a range of frames
    record["delete_frames"] = [recordPlayer](int startFrame, int count) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.delete_frames: RecordPlayer not available");
        }
        if (startFrame < 0) {
            throw sol::error("record.delete_frames: start_frame must be non-negative");
        }
        if (count < 0) {
            throw sol::error("record.delete_frames: count must be non-negative");
        }
        return recordPlayer->DeleteFrames(static_cast<size_t>(startFrame), static_cast<size_t>(count));
    };

    // tas.record.copy_frames(src_start, dest_start, count) - Copy frames
    record["copy_frames"] = [recordPlayer](int srcStart, int destStart, int count) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.copy_frames: RecordPlayer not available");
        }
        if (srcStart < 0) {
            throw sol::error("record.copy_frames: src_start must be non-negative");
        }
        if (destStart < 0) {
            throw sol::error("record.copy_frames: dest_start must be non-negative");
        }
        if (count < 0) {
            throw sol::error("record.copy_frames: count must be non-negative");
        }
        return recordPlayer->CopyFrames(static_cast<size_t>(srcStart), static_cast<size_t>(destStart),
                                        static_cast<size_t>(count));
    };

    // tas.record.duplicate_frame(frame, count) - Duplicate a single frame multiple times
    record["duplicate_frame"] = [recordPlayer](int frame, int count) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.duplicate_frame: RecordPlayer not available");
        }
        if (frame < 0) {
            throw sol::error("record.duplicate_frame: frame must be non-negative");
        }
        if (count < 0) {
            throw sol::error("record.duplicate_frame: count must be non-negative");
        }
        return recordPlayer->DuplicateFrame(static_cast<size_t>(frame), static_cast<size_t>(count));
    };

    // ===================================================================
    // Advanced Playback Control APIs
    // ===================================================================

    // tas.record.set_playback_speed(speed) - Set playback speed multiplier
    record["set_playback_speed"] = [recordPlayer](float speed) {
        if (!recordPlayer) {
            throw sol::error("record.set_playback_speed: RecordPlayer not available");
        }
        if (speed <= 0.0f) {
            throw sol::error("record.set_playback_speed: speed must be positive");
        }
        recordPlayer->SetPlaybackSpeed(speed);
    };

    // tas.record.get_playback_speed() - Get current playback speed multiplier
    record["get_playback_speed"] = [recordPlayer]() -> float {
        if (!recordPlayer) {
            return 1.0f;
        }
        return recordPlayer->GetPlaybackSpeed();
    };

    // tas.record.seek_relative(offset) - Seek relative to current position
    record["seek_relative"] = [recordPlayer](int offset) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.seek_relative: RecordPlayer not available");
        }
        return recordPlayer->SeekRelative(offset);
    };

    // tas.record.is_at_end() - Check if at the last frame
    record["is_at_end"] = [recordPlayer]() -> bool {
        if (!recordPlayer) {
            return false;
        }
        return recordPlayer->IsAtEnd();
    };

    // tas.record.get_progress() - Get playback progress (0.0 - 1.0)
    record["get_progress"] = [recordPlayer]() -> float {
        if (!recordPlayer) {
            return 0.0f;
        }
        return recordPlayer->GetProgress();
    };

    // ===================================================================
    // Input Display & Analysis APIs
    // ===================================================================

    // tas.record.get_input_string(frame) - Get human-readable input string
    record["get_input_string"] = [recordPlayer](int frame) -> std::string {
        if (!recordPlayer) {
            return "";
        }
        if (frame < 0) {
            return "";
        }
        return recordPlayer->GetInputString(static_cast<size_t>(frame));
    };

    // tas.record.get_pressed_keys(frame) - Get list of pressed keys for a frame
    record["get_pressed_keys"] = [recordPlayer, context](int frame) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }
        if (frame < 0) {
            return sol::nil;
        }

        auto keys = recordPlayer->GetPressedKeys(static_cast<size_t>(frame));
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();
        for (size_t i = 0; i < keys.size(); i++) {
            result[i + 1] = keys[i]; // Lua arrays are 1-indexed
        }
        return result;
    };

    // tas.record.find_input_change(start_frame, key) - Find next frame where key state changes
    record["find_input_change"] = [recordPlayer](int startFrame, const std::string &key) -> int {
        if (!recordPlayer) {
            return -1;
        }
        if (startFrame < 0) {
            return -1;
        }
        if (key.empty()) {
            throw sol::error("record.find_input_change: key cannot be empty");
        }
        return recordPlayer->FindInputChange(static_cast<size_t>(startFrame), key);
    };

    // ===================================================================
    // Validation & Comparison APIs
    // ===================================================================

    // tas.record.compare_frames(frame1, frame2) - Compare two frames
    record["compare_frames"] = [recordPlayer](int frame1, int frame2) -> bool {
        if (!recordPlayer) {
            return false;
        }
        if (frame1 < 0 || frame2 < 0) {
            return false;
        }
        return recordPlayer->CompareFrames(static_cast<size_t>(frame1), static_cast<size_t>(frame2));
    };

    // tas.record.validate() - Validate record integrity
    record["validate"] = [recordPlayer]() -> bool {
        if (!recordPlayer) {
            throw sol::error("record.validate: RecordPlayer not available");
        }
        return recordPlayer->Validate();
    };

    // ===================================================================
    // Save & Export Operations APIs
    // ===================================================================

    // tas.record.save(path) - Save the current record
    record["save"] = [recordPlayer](const std::string &path) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.save: RecordPlayer not available");
        }
        if (path.empty()) {
            throw sol::error("record.save: path cannot be empty");
        }
        return recordPlayer->Save(path);
    };

    // tas.record.export_inputs(path, format) - Export inputs to text/JSON
    record["export_inputs"] = [recordPlayer](const std::string &path, const std::string &format) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.export_inputs: RecordPlayer not available");
        }
        if (path.empty()) {
            throw sol::error("record.export_inputs: path cannot be empty");
        }
        if (format.empty()) {
            throw sol::error("record.export_inputs: format cannot be empty");
        }
        return recordPlayer->ExportInputs(path, format);
    };

    // ===================================================================
    // Section Management APIs
    // ===================================================================

    // tas.record.add_section(start_frame, end_frame, name, [description], [color])
    record["add_section"] = [recordPlayer](int startFrame, int endFrame, const std::string &name,
                                           sol::optional<std::string> description,
                                           sol::optional<std::string> color) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.add_section: RecordPlayer not available");
        }
        return recordPlayer->AddSection(static_cast<size_t>(startFrame), static_cast<size_t>(endFrame),
                                        name, description.value_or(""), color.value_or(""));
    };

    // tas.record.remove_section(name)
    record["remove_section"] = [recordPlayer](const std::string &name) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.remove_section: RecordPlayer not available");
        }
        return recordPlayer->RemoveSection(name);
    };

    // tas.record.get_section(name)
    record["get_section"] = [recordPlayer, context](const std::string &name) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }
        const RecordSection *section = recordPlayer->GetSection(name);
        if (!section) {
            return sol::nil;
        }

        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();
        result["name"] = section->name;
        result["description"] = section->description;
        result["start_frame"] = section->startFrame;
        result["end_frame"] = section->endFrame;
        result["color"] = section->color;
        return result;
    };

    // tas.record.get_sections()
    record["get_sections"] = [recordPlayer, context]() -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        auto sections = recordPlayer->GetAllSections();
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();

        for (size_t i = 0; i < sections.size(); ++i) {
            sol::table section = lua.create_table();
            section["name"] = sections[i].name;
            section["description"] = sections[i].description;
            section["start_frame"] = sections[i].startFrame;
            section["end_frame"] = sections[i].endFrame;
            section["color"] = sections[i].color;
            result[i + 1] = section;
        }

        return result;
    };

    // tas.record.get_section_at_frame(frame)
    record["get_section_at_frame"] = [recordPlayer, context](int frame) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        const RecordSection *section = recordPlayer->GetSectionAtFrame(static_cast<size_t>(frame));
        if (!section) {
            return sol::nil;
        }

        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();
        result["name"] = section->name;
        result["description"] = section->description;
        result["start_frame"] = section->startFrame;
        result["end_frame"] = section->endFrame;
        result["color"] = section->color;
        return result;
    };

    // tas.record.rename_section(old_name, new_name)
    record["rename_section"] = [recordPlayer](const std::string &oldName, const std::string &newName) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.rename_section: RecordPlayer not available");
        }
        return recordPlayer->RenameSection(oldName, newName);
    };

    // tas.record.move_section(name, new_start_frame)
    record["move_section"] = [recordPlayer](const std::string &name, int newStartFrame) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.move_section: RecordPlayer not available");
        }
        return recordPlayer->MoveSection(name, static_cast<size_t>(newStartFrame));
    };

    // tas.record.resize_section(name, new_start_frame, new_end_frame)
    record["resize_section"] = [recordPlayer](const std::string &name, int newStartFrame, int newEndFrame) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.resize_section: RecordPlayer not available");
        }
        return recordPlayer->ResizeSection(name, static_cast<size_t>(newStartFrame), static_cast<size_t>(newEndFrame));
    };

    // ===================================================================
    // Marker Management APIs
    // ===================================================================

    // Register MarkerType enum
    sol::table markerTypeTable = record.create();
    markerTypeTable["BOOKMARK"] = static_cast<int>(MarkerType::Bookmark);
    markerTypeTable["CHECKPOINT"] = static_cast<int>(MarkerType::Checkpoint);
    markerTypeTable["BUG"] = static_cast<int>(MarkerType::Bug);
    markerTypeTable["TRICK"] = static_cast<int>(MarkerType::Trick);
    markerTypeTable["SYNC"] = static_cast<int>(MarkerType::Sync);
    markerTypeTable["CUSTOM"] = static_cast<int>(MarkerType::Custom);
    record["MarkerType"] = markerTypeTable;

    // tas.record.add_marker(frame, name, [type], [description], [color])
    record["add_marker"] = [recordPlayer](int frame, const std::string &name,
                                          sol::optional<int> type, sol::optional<std::string> description,
                                          sol::optional<std::string> color) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.add_marker: RecordPlayer not available");
        }
        MarkerType markerType = static_cast<MarkerType>(type.value_or(0));
        return recordPlayer->AddMarker(static_cast<size_t>(frame), name, markerType,
                                       description.value_or(""), color.value_or(""));
    };

    // tas.record.remove_marker(name)
    record["remove_marker"] = [recordPlayer](const std::string &name) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.remove_marker: RecordPlayer not available");
        }
        return recordPlayer->RemoveMarker(name);
    };

    // tas.record.get_marker(name)
    record["get_marker"] = [recordPlayer, context](const std::string &name) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        const RecordMarker *marker = recordPlayer->GetMarker(name);
        if (!marker) {
            return sol::nil;
        }

        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();
        result["name"] = marker->name;
        result["frame"] = marker->frame;
        result["type"] = static_cast<int>(marker->type);
        result["description"] = marker->description;
        result["color"] = marker->color;
        return result;
    };

    // tas.record.get_markers()
    record["get_markers"] = [recordPlayer, context]() -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        auto markers = recordPlayer->GetAllMarkers();
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();

        for (size_t i = 0; i < markers.size(); ++i) {
            sol::table marker = lua.create_table();
            marker["name"] = markers[i].name;
            marker["frame"] = markers[i].frame;
            marker["type"] = static_cast<int>(markers[i].type);
            marker["description"] = markers[i].description;
            marker["color"] = markers[i].color;
            result[i + 1] = marker;
        }

        return result;
    };

    // tas.record.get_markers_at_frame(frame)
    record["get_markers_at_frame"] = [recordPlayer, context](int frame) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        auto markers = recordPlayer->GetMarkersAtFrame(static_cast<size_t>(frame));
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();

        for (size_t i = 0; i < markers.size(); ++i) {
            sol::table marker = lua.create_table();
            marker["name"] = markers[i].name;
            marker["frame"] = markers[i].frame;
            marker["type"] = static_cast<int>(markers[i].type);
            marker["description"] = markers[i].description;
            marker["color"] = markers[i].color;
            result[i + 1] = marker;
        }

        return result;
    };

    // tas.record.get_markers_in_range(start_frame, end_frame)
    record["get_markers_in_range"] = [recordPlayer, context](int startFrame, int endFrame) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        auto markers = recordPlayer->GetMarkersInRange(static_cast<size_t>(startFrame), static_cast<size_t>(endFrame));
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();

        for (size_t i = 0; i < markers.size(); ++i) {
            sol::table marker = lua.create_table();
            marker["name"] = markers[i].name;
            marker["frame"] = markers[i].frame;
            marker["type"] = static_cast<int>(markers[i].type);
            marker["description"] = markers[i].description;
            marker["color"] = markers[i].color;
            result[i + 1] = marker;
        }

        return result;
    };

    // tas.record.rename_marker(old_name, new_name)
    record["rename_marker"] = [recordPlayer](const std::string &oldName, const std::string &newName) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.rename_marker: RecordPlayer not available");
        }
        return recordPlayer->RenameMarker(oldName, newName);
    };

    // tas.record.move_marker(name, new_frame)
    record["move_marker"] = [recordPlayer](const std::string &name, int newFrame) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.move_marker: RecordPlayer not available");
        }
        return recordPlayer->MoveMarker(name, static_cast<size_t>(newFrame));
    };

    // tas.record.seek_to_marker(name)
    record["seek_to_marker"] = [recordPlayer](const std::string &name) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.seek_to_marker: RecordPlayer not available");
        }
        return recordPlayer->SeekToMarker(name);
    };

    // tas.record.get_next_marker(current_frame, [type])
    record["get_next_marker"] = [recordPlayer, context](int currentFrame, sol::optional<int> type) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        MarkerType markerType = static_cast<MarkerType>(type.value_or(0));
        const RecordMarker *marker = recordPlayer->GetNextMarker(static_cast<size_t>(currentFrame), markerType);
        if (!marker) {
            return sol::nil;
        }

        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();
        result["name"] = marker->name;
        result["frame"] = marker->frame;
        result["type"] = static_cast<int>(marker->type);
        result["description"] = marker->description;
        result["color"] = marker->color;
        return result;
    };

    // tas.record.get_previous_marker(current_frame, [type])
    record["get_previous_marker"] = [recordPlayer, context](int currentFrame, sol::optional<int> type) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        MarkerType markerType = static_cast<MarkerType>(type.value_or(0));
        const RecordMarker *marker = recordPlayer->GetPreviousMarker(static_cast<size_t>(currentFrame), markerType);
        if (!marker) {
            return sol::nil;
        }

        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();
        result["name"] = marker->name;
        result["frame"] = marker->frame;
        result["type"] = static_cast<int>(marker->type);
        result["description"] = marker->description;
        result["color"] = marker->color;
        return result;
    };

    // ===================================================================
    // Comment Management APIs
    // ===================================================================

    // tas.record.add_comment(frame, text, [author], [category])
    record["add_comment"] = [recordPlayer](int frame, const std::string &text,
                                           sol::optional<std::string> author,
                                           sol::optional<std::string> category) -> int {
        if (!recordPlayer) {
            throw sol::error("record.add_comment: RecordPlayer not available");
        }
        return static_cast<int>(recordPlayer->AddComment(static_cast<size_t>(frame), text,
                                                         author.value_or(""), category.value_or("")));
    };

    // tas.record.add_range_comment(start_frame, end_frame, text, [author], [category])
    record["add_range_comment"] = [recordPlayer](int startFrame, int endFrame, const std::string &text,
                                                 sol::optional<std::string> author,
                                                 sol::optional<std::string> category) -> int {
        if (!recordPlayer) {
            throw sol::error("record.add_range_comment: RecordPlayer not available");
        }
        return static_cast<int>(recordPlayer->AddRangeComment(static_cast<size_t>(startFrame),
                                                              static_cast<size_t>(endFrame),
                                                              text, author.value_or(""), category.value_or("")));
    };

    // tas.record.remove_comment(index)
    record["remove_comment"] = [recordPlayer](int index) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.remove_comment: RecordPlayer not available");
        }
        return recordPlayer->RemoveComment(static_cast<size_t>(index));
    };

    // tas.record.get_comment(index)
    record["get_comment"] = [recordPlayer, context](int index) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        const RecordComment *comment = recordPlayer->GetComment(static_cast<size_t>(index));
        if (!comment) {
            return sol::nil;
        }

        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();
        result["text"] = comment->text;
        result["start_frame"] = comment->startFrame;
        result["end_frame"] = comment->endFrame;
        result["author"] = comment->author;
        result["timestamp"] = comment->timestamp;
        result["category"] = comment->category;
        return result;
    };

    // tas.record.get_comments_at_frame(frame)
    record["get_comments_at_frame"] = [recordPlayer, context](int frame) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        auto comments = recordPlayer->GetCommentsAtFrame(static_cast<size_t>(frame));
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();

        for (size_t i = 0; i < comments.size(); ++i) {
            sol::table comment = lua.create_table();
            comment["text"] = comments[i]->text;
            comment["start_frame"] = comments[i]->startFrame;
            comment["end_frame"] = comments[i]->endFrame;
            comment["author"] = comments[i]->author;
            comment["timestamp"] = comments[i]->timestamp;
            comment["category"] = comments[i]->category;
            result[i + 1] = comment;
        }

        return result;
    };

    // tas.record.get_all_comments()
    record["get_all_comments"] = [recordPlayer, context]() -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        auto comments = recordPlayer->GetAllComments();
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();

        for (size_t i = 0; i < comments.size(); ++i) {
            sol::table comment = lua.create_table();
            comment["text"] = comments[i].text;
            comment["start_frame"] = comments[i].startFrame;
            comment["end_frame"] = comments[i].endFrame;
            comment["author"] = comments[i].author;
            comment["timestamp"] = comments[i].timestamp;
            comment["category"] = comments[i].category;
            result[i + 1] = comment;
        }

        return result;
    };

    // tas.record.edit_comment(index, new_text)
    record["edit_comment"] = [recordPlayer](int index, const std::string &newText) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.edit_comment: RecordPlayer not available");
        }
        return recordPlayer->EditComment(static_cast<size_t>(index), newText);
    };

    // ===================================================================
    // Macro Management APIs
    // ===================================================================

    // tas.record.save_macro(name, start_frame, end_frame, [description])
    record["save_macro"] = [recordPlayer](const std::string &name, int startFrame, int endFrame,
                                          sol::optional<std::string> description) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.save_macro: RecordPlayer not available");
        }
        return recordPlayer->SaveMacro(name, static_cast<size_t>(startFrame), static_cast<size_t>(endFrame),
                                       description.value_or(""));
    };

    // tas.record.delete_macro(name)
    record["delete_macro"] = [recordPlayer](const std::string &name) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.delete_macro: RecordPlayer not available");
        }
        return recordPlayer->DeleteMacro(name);
    };

    // tas.record.get_macro(name)
    record["get_macro"] = [recordPlayer, context](const std::string &name) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        const RecordMacro *macro = recordPlayer->GetMacro(name);
        if (!macro) {
            return sol::nil;
        }

        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();
        result["name"] = macro->name;
        result["description"] = macro->description;
        result["frame_count"] = macro->frames.size();
        return result;
    };

    // tas.record.get_macros()
    record["get_macros"] = [recordPlayer, context]() -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        auto macros = recordPlayer->GetAllMacros();
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();

        for (size_t i = 0; i < macros.size(); ++i) {
            sol::table macro = lua.create_table();
            macro["name"] = macros[i].name;
            macro["description"] = macros[i].description;
            macro["frame_count"] = macros[i].frames.size();
            result[i + 1] = macro;
        }

        return result;
    };

    // tas.record.insert_macro(name, at_frame, [repeat_count])
    record["insert_macro"] = [recordPlayer](const std::string &name, int atFrame,
                                            sol::optional<int> repeatCount) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.insert_macro: RecordPlayer not available");
        }
        return recordPlayer->InsertMacro(name, static_cast<size_t>(atFrame),
                                         static_cast<size_t>(repeatCount.value_or(1)));
    };

    // ===================================================================
    // Metadata Management APIs
    // ===================================================================

    // tas.record.set_metadata(field, value)
    record["set_metadata"] = [recordPlayer](const std::string &field, const std::string &value) {
        if (!recordPlayer) {
            throw sol::error("record.set_metadata: RecordPlayer not available");
        }
        recordPlayer->SetMetadataField(field, value);
    };

    // tas.record.get_metadata(field)
    record["get_metadata"] = [recordPlayer](const std::string &field) -> std::string {
        if (!recordPlayer) {
            return "";
        }
        return recordPlayer->GetMetadataField(field);
    };

    // tas.record.add_tag(tag)
    record["add_tag"] = [recordPlayer](const std::string &tag) {
        if (!recordPlayer) {
            throw sol::error("record.add_tag: RecordPlayer not available");
        }
        recordPlayer->AddTag(tag);
    };

    // tas.record.remove_tag(tag)
    record["remove_tag"] = [recordPlayer](const std::string &tag) {
        if (!recordPlayer) {
            throw sol::error("record.remove_tag: RecordPlayer not available");
        }
        recordPlayer->RemoveTag(tag);
    };

    // tas.record.get_tags()
    record["get_tags"] = [recordPlayer, context]() -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        auto tags = recordPlayer->GetTags();
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();

        for (size_t i = 0; i < tags.size(); ++i) {
            result[i + 1] = tags[i];
        }

        return result;
    };

    // tas.record.increment_rerecord_count()
    record["increment_rerecord_count"] = [recordPlayer]() {
        if (!recordPlayer) {
            throw sol::error("record.increment_rerecord_count: RecordPlayer not available");
        }
        recordPlayer->IncrementRerecordCount();
    };

    // tas.record.get_rerecord_count()
    record["get_rerecord_count"] = [recordPlayer]() -> int {
        if (!recordPlayer) {
            return 0;
        }
        return static_cast<int>(recordPlayer->GetRerecordCount());
    };

    // ===================================================================
    // Undo/Redo APIs
    // ===================================================================

    // tas.record.undo()
    record["undo"] = [recordPlayer]() -> bool {
        if (!recordPlayer) {
            throw sol::error("record.undo: RecordPlayer not available");
        }
        return recordPlayer->Undo();
    };

    // tas.record.redo()
    record["redo"] = [recordPlayer]() -> bool {
        if (!recordPlayer) {
            throw sol::error("record.redo: RecordPlayer not available");
        }
        return recordPlayer->Redo();
    };

    // tas.record.can_undo()
    record["can_undo"] = [recordPlayer]() -> bool {
        if (!recordPlayer) {
            return false;
        }
        return recordPlayer->CanUndo();
    };

    // tas.record.can_redo()
    record["can_redo"] = [recordPlayer]() -> bool {
        if (!recordPlayer) {
            return false;
        }
        return recordPlayer->CanRedo();
    };

    // tas.record.get_undo_description()
    record["get_undo_description"] = [recordPlayer]() -> std::string {
        if (!recordPlayer) {
            return "";
        }
        return recordPlayer->GetUndoDescription();
    };

    // tas.record.get_redo_description()
    record["get_redo_description"] = [recordPlayer]() -> std::string {
        if (!recordPlayer) {
            return "";
        }
        return recordPlayer->GetRedoDescription();
    };

    // tas.record.clear_history()
    record["clear_history"] = [recordPlayer]() {
        if (!recordPlayer) {
            throw sol::error("record.clear_history: RecordPlayer not available");
        }
        recordPlayer->ClearHistory();
    };

    // tas.record.get_history_size()
    record["get_history_size"] = [recordPlayer]() -> int {
        if (!recordPlayer) {
            return 0;
        }
        return static_cast<int>(recordPlayer->GetHistorySize());
    };

    // tas.record.set_max_history_size(size)
    record["set_max_history_size"] = [recordPlayer](int size) {
        if (!recordPlayer) {
            throw sol::error("record.set_max_history_size: RecordPlayer not available");
        }
        recordPlayer->SetMaxHistorySize(static_cast<size_t>(size));
    };

    // ===================================================================
    // Search & Filter APIs
    // ===================================================================

    // tas.record.find_frames_with_keys(key_string, [start_frame], [end_frame])
    record["find_frames_with_keys"] = [recordPlayer, context](const std::string &keyString,
                                                              sol::optional<int> startFrame,
                                                              sol::optional<int> endFrame) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        auto frames = recordPlayer->FindFramesWithKeys(keyString,
                                                       static_cast<size_t>(startFrame.value_or(0)),
                                                       static_cast<size_t>(endFrame.value_or(-1)));

        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();

        for (size_t i = 0; i < frames.size(); ++i) {
            result[i + 1] = frames[i];
        }

        return result;
    };

    // tas.record.find_frames_by_delta_time(delta_time, [tolerance])
    record["find_frames_by_delta_time"] = [recordPlayer, context](float deltaTime,
                                                                  sol::optional<float> tolerance) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        auto frames = recordPlayer->FindFramesByDeltaTime(deltaTime, tolerance.value_or(0.01f));

        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();

        for (size_t i = 0; i < frames.size(); ++i) {
            result[i + 1] = frames[i];
        }

        return result;
    };

    // ===================================================================
    // Statistics & Analysis APIs
    // ===================================================================

    // tas.record.get_statistics([start_frame], [end_frame])
    record["get_statistics"] = [recordPlayer, context](sol::optional<int> startFrame,
                                                       sol::optional<int> endFrame) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        auto stats = recordPlayer->GetStatistics(static_cast<size_t>(startFrame.value_or(0)),
                                                 static_cast<size_t>(endFrame.value_or(-1)));

        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();

        for (const auto &pair : stats) {
            result[pair.first] = pair.second;
        }

        return result;
    };

    // ===================================================================
    // Branch Management APIs
    // ===================================================================

    // tas.record.create_branch(name, divergence_frame, [description])
    record["create_branch"] = [recordPlayer](const std::string &name, int divergenceFrame,
                                             sol::optional<std::string> description) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.create_branch: RecordPlayer not available");
        }
        return recordPlayer->CreateBranch(name, static_cast<size_t>(divergenceFrame), description.value_or(""));
    };

    // tas.record.delete_branch(name)
    record["delete_branch"] = [recordPlayer](const std::string &name) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.delete_branch: RecordPlayer not available");
        }
        return recordPlayer->DeleteBranch(name);
    };

    // tas.record.switch_branch(name)
    record["switch_branch"] = [recordPlayer](const std::string &name) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.switch_branch: RecordPlayer not available");
        }
        return recordPlayer->SwitchBranch(name);
    };

    // tas.record.get_current_branch()
    record["get_current_branch"] = [recordPlayer]() -> std::string {
        if (!recordPlayer) {
            return "";
        }
        return recordPlayer->GetCurrentBranch();
    };

    // tas.record.get_branches()
    record["get_branches"] = [recordPlayer, context]() -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        auto branches = recordPlayer->GetAllBranches();
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();

        for (size_t i = 0; i < branches.size(); ++i) {
            sol::table branch = lua.create_table();
            branch["name"] = branches[i].name;
            branch["description"] = branches[i].description;
            branch["divergence_frame"] = branches[i].divergenceFrame;
            branch["convergence_frame"] = branches[i].convergenceFrame;
            branch["parent_branch"] = branches[i].parentBranch;
            branch["frame_count"] = branches[i].frames.size();
            result[i + 1] = branch;
        }

        return result;
    };

    // ===================================================================
    // Savestate Linking APIs
    // ===================================================================

    // tas.record.link_savestate(frame, savestate_path, [description])
    record["link_savestate"] = [recordPlayer](int frame, const std::string &savestatePath,
                                              sol::optional<std::string> description) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.link_savestate: RecordPlayer not available");
        }
        return recordPlayer->LinkSavestate(static_cast<size_t>(frame), savestatePath, description.value_or(""));
    };

    // tas.record.unlink_savestate(frame)
    record["unlink_savestate"] = [recordPlayer](int frame) -> bool {
        if (!recordPlayer) {
            throw sol::error("record.unlink_savestate: RecordPlayer not available");
        }
        return recordPlayer->UnlinkSavestate(static_cast<size_t>(frame));
    };

    // tas.record.get_savestate_link(frame)
    record["get_savestate_link"] = [recordPlayer, context](int frame) -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        const SavestateLink *link = recordPlayer->GetSavestateLink(static_cast<size_t>(frame));
        if (!link) {
            return sol::nil;
        }

        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();
        result["frame"] = link->frame;
        result["path"] = link->savestatePath;
        result["description"] = link->description;
        result["created_at"] = link->createdAt;
        return result;
    };

    // tas.record.get_all_savestate_links()
    record["get_all_savestate_links"] = [recordPlayer, context]() -> sol::object {
        if (!recordPlayer) {
            return sol::nil;
        }

        auto links = recordPlayer->GetAllSavestateLinks();
        auto &lua = context->GetLuaState();
        sol::table result = lua.create_table();

        for (size_t i = 0; i < links.size(); ++i) {
            sol::table link = lua.create_table();
            link["frame"] = links[i].frame;
            link["path"] = links[i].savestatePath;
            link["description"] = links[i].description;
            link["created_at"] = links[i].createdAt;
            result[i + 1] = link;
        }

        return result;
    };
}
