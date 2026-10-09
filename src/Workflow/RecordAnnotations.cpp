// Sidecar persistence for everything a .tas file has no room for: sections, markers,
// comments, macros, metadata, savestate links and inactive branches.
#include "RecordPlayer.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

#include <yyjson.h>

#include "Logger.h"

namespace {
constexpr int kAnnotationsVersion = 1;

yyjson_mut_val *FramesToJson(yyjson_mut_doc *doc, const std::vector<RecordFrameData> &frames) {
    yyjson_mut_val *array = yyjson_mut_arr(doc);
    for (const auto &frame : frames) {
        yyjson_mut_val *pair = yyjson_mut_arr(doc);
        yyjson_mut_arr_add_real(doc, pair, frame.deltaTime);
        yyjson_mut_arr_add_int(doc, pair, frame.keyStates);
        yyjson_mut_arr_append(array, pair);
    }
    return array;
}

std::vector<RecordFrameData> FramesFromJson(yyjson_val *array) {
    std::vector<RecordFrameData> frames;
    if (!yyjson_is_arr(array)) {
        return frames;
    }
    frames.reserve(yyjson_arr_size(array));
    size_t index, count;
    yyjson_val *pair;
    yyjson_arr_foreach(array, index, count, pair) {
        RecordFrameData frame;
        frame.deltaTime = static_cast<float>(yyjson_get_num(yyjson_arr_get(pair, 0)));
        frame.keyStates = static_cast<int>(yyjson_get_int(yyjson_arr_get(pair, 1)));
        frames.push_back(frame);
    }
    return frames;
}

void AddStringMap(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key,
                  const std::unordered_map<std::string, std::string> &map) {
    yyjson_mut_val *child = yyjson_mut_obj(doc);
    for (const auto &[name, value] : map) {
        yyjson_mut_obj_add(child, yyjson_mut_strcpy(doc, name.c_str()), yyjson_mut_strcpy(doc, value.c_str()));
    }
    yyjson_mut_obj_add_val(doc, obj, key, child);
}

std::unordered_map<std::string, std::string> ReadStringMap(yyjson_val *obj) {
    std::unordered_map<std::string, std::string> map;
    if (!yyjson_is_obj(obj)) {
        return map;
    }
    size_t index, count;
    yyjson_val *key, *value;
    yyjson_obj_foreach(obj, index, count, key, value) {
        if (yyjson_is_str(value)) {
            map[yyjson_get_str(key)] = yyjson_get_str(value);
        }
    }
    return map;
}

std::string ReadString(yyjson_val *obj, const char *key) {
    const char *value = yyjson_get_str(yyjson_obj_get(obj, key));
    return value ? value : "";
}

size_t ReadSize(yyjson_val *obj, const char *key) {
    return static_cast<size_t>(yyjson_get_uint(yyjson_obj_get(obj, key)));
}

void AddStr(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const std::string &value) {
    yyjson_mut_obj_add_strcpy(doc, obj, key, value.c_str());
}
} // namespace

std::string RecordPlayer::GetAnnotationsPath(const std::string &recordPath) {
    return recordPath + ".notes.json";
}

bool RecordPlayer::HasAnnotations() const {
    return !m_Sections.empty() || !m_Markers.empty() || !m_Comments.empty() || !m_Macros.empty() ||
           !m_Branches.empty() || !m_SavestateLinks.empty() || !m_Metadata.title.empty() ||
           !m_Metadata.author.empty() || !m_Metadata.description.empty() || !m_Metadata.tags.empty() ||
           !m_Metadata.customFields.empty() || m_Metadata.rerecordCount > 0;
}

bool RecordPlayer::SaveAnnotations(const std::string &path) const {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_int(doc, root, "version", kAnnotationsVersion);

    yyjson_mut_val *metadata = yyjson_mut_obj(doc);
    AddStr(doc, metadata, "title", m_Metadata.title);
    AddStr(doc, metadata, "author", m_Metadata.author);
    AddStr(doc, metadata, "description", m_Metadata.description);
    AddStr(doc, metadata, "game_version", m_Metadata.gameVersion);
    AddStr(doc, metadata, "tas_version", m_Metadata.tasVersion);
    yyjson_mut_obj_add_int(doc, metadata, "created_at", static_cast<int64_t>(m_Metadata.createdAt));
    yyjson_mut_obj_add_int(doc, metadata, "modified_at", static_cast<int64_t>(m_Metadata.modifiedAt));
    yyjson_mut_obj_add_uint(doc, metadata, "rerecord_count", m_Metadata.rerecordCount);
    yyjson_mut_val *tags = yyjson_mut_arr(doc);
    for (const auto &tag : m_Metadata.tags) {
        yyjson_mut_arr_add_strcpy(doc, tags, tag.c_str());
    }
    yyjson_mut_obj_add_val(doc, metadata, "tags", tags);
    AddStringMap(doc, metadata, "custom", m_Metadata.customFields);
    yyjson_mut_obj_add_val(doc, root, "metadata", metadata);

    yyjson_mut_val *sections = yyjson_mut_arr(doc);
    for (const auto &[name, section] : m_Sections) {
        yyjson_mut_val *item = yyjson_mut_obj(doc);
        AddStr(doc, item, "name", section.name);
        AddStr(doc, item, "description", section.description);
        AddStr(doc, item, "color", section.color);
        yyjson_mut_obj_add_uint(doc, item, "start_frame", section.startFrame);
        yyjson_mut_obj_add_uint(doc, item, "end_frame", section.endFrame);
        AddStringMap(doc, item, "metadata", section.metadata);
        yyjson_mut_arr_append(sections, item);
    }
    yyjson_mut_obj_add_val(doc, root, "sections", sections);

    yyjson_mut_val *markers = yyjson_mut_arr(doc);
    for (const auto &[name, marker] : m_Markers) {
        yyjson_mut_val *item = yyjson_mut_obj(doc);
        AddStr(doc, item, "name", marker.name);
        yyjson_mut_obj_add_uint(doc, item, "frame", marker.frame);
        yyjson_mut_obj_add_int(doc, item, "type", static_cast<int>(marker.type));
        AddStr(doc, item, "description", marker.description);
        AddStr(doc, item, "color", marker.color);
        AddStringMap(doc, item, "metadata", marker.metadata);
        yyjson_mut_arr_append(markers, item);
    }
    yyjson_mut_obj_add_val(doc, root, "markers", markers);

    yyjson_mut_val *comments = yyjson_mut_arr(doc);
    for (const auto &comment : m_Comments) {
        yyjson_mut_val *item = yyjson_mut_obj(doc);
        AddStr(doc, item, "text", comment.text);
        yyjson_mut_obj_add_uint(doc, item, "start_frame", comment.startFrame);
        yyjson_mut_obj_add_uint(doc, item, "end_frame", comment.endFrame);
        AddStr(doc, item, "author", comment.author);
        AddStr(doc, item, "category", comment.category);
        yyjson_mut_obj_add_int(doc, item, "timestamp", static_cast<int64_t>(comment.timestamp));
        yyjson_mut_obj_add_uint(doc, item, "id", comment.id);
        yyjson_mut_arr_append(comments, item);
    }
    yyjson_mut_obj_add_val(doc, root, "comments", comments);

    yyjson_mut_val *macros = yyjson_mut_arr(doc);
    for (const auto &[name, macro] : m_Macros) {
        yyjson_mut_val *item = yyjson_mut_obj(doc);
        AddStr(doc, item, "name", macro.name);
        AddStr(doc, item, "description", macro.description);
        yyjson_mut_obj_add_val(doc, item, "frames", FramesToJson(doc, macro.frames));
        AddStringMap(doc, item, "metadata", macro.metadata);
        yyjson_mut_arr_append(macros, item);
    }
    yyjson_mut_obj_add_val(doc, root, "macros", macros);

    yyjson_mut_val *links = yyjson_mut_arr(doc);
    for (const auto &[frame, link] : m_SavestateLinks) {
        yyjson_mut_val *item = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_uint(doc, item, "frame", link.frame);
        AddStr(doc, item, "path", link.savestatePath);
        AddStr(doc, item, "description", link.description);
        yyjson_mut_obj_add_int(doc, item, "created_at", static_cast<int64_t>(link.createdAt));
        yyjson_mut_arr_append(links, item);
    }
    yyjson_mut_obj_add_val(doc, root, "savestate_links", links);

    // The .tas file holds the active branch; every other branch (including main) lives here.
    AddStr(doc, root, "active_branch", m_CurrentBranch);
    yyjson_mut_val *branches = yyjson_mut_arr(doc);
    if (!m_CurrentBranch.empty()) {
        yyjson_mut_val *item = yyjson_mut_obj(doc);
        AddStr(doc, item, "name", "");
        yyjson_mut_obj_add_val(doc, item, "frames", FramesToJson(doc, m_MainBranchFrames));
        yyjson_mut_arr_append(branches, item);
    }
    for (const auto &[name, branch] : m_Branches) {
        yyjson_mut_val *item = yyjson_mut_obj(doc);
        AddStr(doc, item, "name", branch.name);
        AddStr(doc, item, "description", branch.description);
        AddStr(doc, item, "parent", branch.parentBranch);
        yyjson_mut_obj_add_uint(doc, item, "divergence_frame", branch.divergenceFrame);
        if (name != m_CurrentBranch) {
            yyjson_mut_obj_add_val(doc, item, "frames", FramesToJson(doc, branch.frames));
        }
        yyjson_mut_arr_append(branches, item);
    }
    yyjson_mut_obj_add_val(doc, root, "branches", branches);

    size_t length = 0;
    char *json = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY, &length);
    yyjson_mut_doc_free(doc);
    if (!json) {
        Log::Error("Failed to serialize record annotations.");
        return false;
    }

    std::ofstream file(path, std::ios::binary);
    if (file) {
        file.write(json, static_cast<std::streamsize>(length));
    }
    free(json);
    if (!file) {
        Log::Error("Failed to write record annotations: %s", path.c_str());
        return false;
    }
    return true;
}

bool RecordPlayer::LoadAnnotations(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string json = buffer.str();

    yyjson_doc *doc = yyjson_read(json.data(), json.size(), 0);
    yyjson_val *root = doc ? yyjson_doc_get_root(doc) : nullptr;
    if (!yyjson_is_obj(root)) {
        Log::Warn("Ignoring unreadable record annotations: %s", path.c_str());
        yyjson_doc_free(doc);
        return false;
    }

    size_t index, count;
    yyjson_val *item;

    if (yyjson_val *metadata = yyjson_obj_get(root, "metadata"); yyjson_is_obj(metadata)) {
        m_Metadata.title = ReadString(metadata, "title");
        m_Metadata.author = ReadString(metadata, "author");
        m_Metadata.description = ReadString(metadata, "description");
        m_Metadata.gameVersion = ReadString(metadata, "game_version");
        m_Metadata.tasVersion = ReadString(metadata, "tas_version");
        m_Metadata.createdAt = static_cast<std::time_t>(yyjson_get_sint(yyjson_obj_get(metadata, "created_at")));
        m_Metadata.modifiedAt = static_cast<std::time_t>(yyjson_get_sint(yyjson_obj_get(metadata, "modified_at")));
        m_Metadata.rerecordCount = ReadSize(metadata, "rerecord_count");
        m_Metadata.tags.clear();
        yyjson_arr_foreach(yyjson_obj_get(metadata, "tags"), index, count, item) {
            if (yyjson_is_str(item)) {
                m_Metadata.tags.emplace_back(yyjson_get_str(item));
            }
        }
        m_Metadata.customFields = ReadStringMap(yyjson_obj_get(metadata, "custom"));
    }

    yyjson_arr_foreach(yyjson_obj_get(root, "sections"), index, count, item) {
        RecordSection section(ReadString(item, "name"), ReadSize(item, "start_frame"), ReadSize(item, "end_frame"));
        section.description = ReadString(item, "description");
        section.color = ReadString(item, "color");
        section.metadata = ReadStringMap(yyjson_obj_get(item, "metadata"));
        m_Sections[section.name] = section;
    }

    yyjson_arr_foreach(yyjson_obj_get(root, "markers"), index, count, item) {
        const auto type = static_cast<int>(yyjson_get_int(yyjson_obj_get(item, "type")));
        RecordMarker marker(ReadString(item, "name"), ReadSize(item, "frame"),
                            type >= 0 && type <= static_cast<int>(MarkerType::Custom) ? static_cast<MarkerType>(type)
                                                                                    : MarkerType::Bookmark);
        marker.description = ReadString(item, "description");
        marker.color = ReadString(item, "color");
        marker.metadata = ReadStringMap(yyjson_obj_get(item, "metadata"));
        m_Markers[marker.name] = marker;
    }

    yyjson_arr_foreach(yyjson_obj_get(root, "comments"), index, count, item) {
        RecordComment comment(ReadString(item, "text"), ReadSize(item, "start_frame"));
        comment.endFrame = ReadSize(item, "end_frame");
        comment.author = ReadString(item, "author");
        comment.category = ReadString(item, "category");
        comment.timestamp = static_cast<std::time_t>(yyjson_get_sint(yyjson_obj_get(item, "timestamp")));
        comment.id = ReadSize(item, "id");
        if (comment.id == 0) {
            comment.id = m_NextCommentId;
        }
        m_NextCommentId = (std::max)(m_NextCommentId, comment.id + 1);
        m_Comments.push_back(comment);
    }

    yyjson_arr_foreach(yyjson_obj_get(root, "macros"), index, count, item) {
        RecordMacro macro(ReadString(item, "name"), ReadString(item, "description"));
        macro.frames = FramesFromJson(yyjson_obj_get(item, "frames"));
        macro.metadata = ReadStringMap(yyjson_obj_get(item, "metadata"));
        m_Macros[macro.name] = macro;
    }

    yyjson_arr_foreach(yyjson_obj_get(root, "savestate_links"), index, count, item) {
        SavestateLink link(ReadSize(item, "frame"), ReadString(item, "path"));
        link.description = ReadString(item, "description");
        link.createdAt = static_cast<std::time_t>(yyjson_get_sint(yyjson_obj_get(item, "created_at")));
        m_SavestateLinks[link.frame] = link;
    }

    const std::string activeBranch = ReadString(root, "active_branch");
    yyjson_arr_foreach(yyjson_obj_get(root, "branches"), index, count, item) {
        const std::string name = ReadString(item, "name");
        if (name.empty()) {
            m_MainBranchFrames = FramesFromJson(yyjson_obj_get(item, "frames"));
            continue;
        }
        RecordBranch branch;
        branch.name = name;
        branch.description = ReadString(item, "description");
        branch.parentBranch = ReadString(item, "parent");
        branch.divergenceFrame = ReadSize(item, "divergence_frame");
        branch.frames = name == activeBranch ? CopyActiveFrames() : FramesFromJson(yyjson_obj_get(item, "frames"));
        m_Branches[name] = std::move(branch);
    }
    if (!activeBranch.empty() && m_Branches.count(activeBranch)) {
        m_CurrentBranch = activeBranch;
    } else {
        m_MainBranchFrames.clear();
    }

    yyjson_doc_free(doc);
    Log::Info("Loaded record annotations: %s", path.c_str());
    return true;
}
