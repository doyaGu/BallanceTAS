#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "Result.h"

namespace tas::determinism {

struct TracePathRequest {
    std::optional<std::filesystem::path> requestedPath;
    std::filesystem::path projectDirectory;
    std::string projectName;
    std::filesystem::path tasRoot;
};

struct TraceDiff {
    bool identical = false;
    size_t firstDivergenceTick = 0;
    size_t divergentTicks = 0;
    size_t comparedTicks = 0;
    size_t leftTicks = 0;
    size_t rightTicks = 0;
};

Result<std::filesystem::path> ResolveTracePath(const TracePathRequest &request);
Result<TraceDiff> OfflineDiff(const std::filesystem::path &left, const std::filesystem::path &right);

} // namespace tas::determinism
