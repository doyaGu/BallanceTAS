#include "HarnessProtocol.h"

#include <cstdlib>
#include <utility>

#include <yyjson.h>

bool ParseHarnessRequest(std::string_view json, HarnessRequest &request, std::string &error) {
    yyjson_read_err readError{};
    yyjson_doc *doc = yyjson_read_opts(const_cast<char *>(json.data()), json.size(), 0, nullptr, &readError);
    if (!doc) {
        error = "invalid request JSON: " + std::string(readError.msg ? readError.msg : "unknown parse error");
        return false;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        error = "request root must be an object";
        return false;
    }

    yyjson_val *protocol = yyjson_obj_get(root, "protocol");
    yyjson_val *project = yyjson_obj_get(root, "project");
    yyjson_val *maxTicks = yyjson_obj_get(root, "max_ticks");

    if (!yyjson_is_uint(protocol) || yyjson_get_uint(protocol) != 1) {
        yyjson_doc_free(doc);
        error = "request protocol must be 1";
        return false;
    }
    if (!yyjson_is_str(project) || yyjson_get_len(project) == 0) {
        yyjson_doc_free(doc);
        error = "request project must be a non-empty string";
        return false;
    }

    HarnessRequest parsed;
    parsed.project.assign(yyjson_get_str(project), yyjson_get_len(project));
    if (maxTicks) {
        const uint64_t value = yyjson_is_uint(maxTicks) ? yyjson_get_uint(maxTicks) : 0;
        if (value == 0 || value > 10000000) {
            yyjson_doc_free(doc);
            error = "request max_ticks must be between 1 and 10000000";
            return false;
        }
        parsed.maxTicks = static_cast<size_t>(value);
    }

    yyjson_doc_free(doc);
    request = std::move(parsed);
    return true;
}

std::string BuildHarnessResult(bool passed,
                               const std::string &project,
                               const std::string &message,
                               size_t tick) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_uint(doc, root, "protocol", 1);
    yyjson_mut_obj_add_str(doc, root, "status", passed ? "passed" : "failed");
    yyjson_mut_obj_add_int(doc, root, "exit_code", passed ? 0 : 1);
    yyjson_mut_obj_add_str(doc, root, "project", project.c_str());
    yyjson_mut_obj_add_str(doc, root, "message", message.c_str());
    yyjson_mut_obj_add_uint(doc, root, "tick", static_cast<uint64_t>(tick));

    char *json = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY, nullptr);
    std::string result = json ? json : "";
    std::free(json);
    yyjson_mut_doc_free(doc);
    return result;
}
