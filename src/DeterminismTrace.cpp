#include "DeterminismTrace.h"

#include <algorithm>
#include <fstream>
#include <vector>

namespace tas::determinism {

constexpr uint32_t kMagic = 0x53415442;
constexpr uint32_t kVersion = 1;

static std::string SanitizeFileStem(std::string value) {
    if (value.empty()) {
        return "trace";
    }
    for (char &ch : value) {
        const bool allowed = (ch >= 'a' && ch <= 'z') ||
                             (ch >= 'A' && ch <= 'Z') ||
                             (ch >= '0' && ch <= '9') ||
                             ch == '-' || ch == '_';
        if (!allowed) {
            ch = '_';
        }
    }
    return value;
}

static std::filesystem::path Normalize(std::filesystem::path path) {
    return path.lexically_normal();
}

static Result<std::vector<uint64_t>> ReadHashes(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return Result<std::vector<uint64_t>>::Error("Failed to open: " + path.string(), "file_open_failed");
    }

    uint32_t magic = 0;
    file.read(reinterpret_cast<char *>(&magic), sizeof(magic));
    if (magic != kMagic) {
        return Result<std::vector<uint64_t>>::Error("Invalid magic in: " + path.string(), "invalid_magic");
    }

    uint32_t version = 0;
    file.read(reinterpret_cast<char *>(&version), sizeof(version));
    if (version != kVersion) {
        return Result<std::vector<uint64_t>>::Error("Unsupported version in: " + path.string(), "unsupported_version");
    }

    uint32_t flags = 0;
    file.read(reinterpret_cast<char *>(&flags), sizeof(flags));
    uint64_t tickCount = 0;
    file.read(reinterpret_cast<char *>(&tickCount), sizeof(tickCount));

    uint16_t nameLen = 0;
    file.read(reinterpret_cast<char *>(&nameLen), sizeof(nameLen));
    if (nameLen > 0) {
        file.seekg(nameLen, std::ios::cur);
    }
    if (!file.good()) {
        return Result<std::vector<uint64_t>>::Error("Failed to read header: " + path.string(), "read_failed");
    }

    std::vector<uint64_t> hashes;
    hashes.reserve(static_cast<size_t>(tickCount));
    for (uint64_t i = 0; i < tickCount; ++i) {
        uint64_t tick = 0;
        uint64_t hash = 0;
        file.read(reinterpret_cast<char *>(&tick), sizeof(tick));
        file.read(reinterpret_cast<char *>(&hash), sizeof(hash));
        if (!file.good()) {
            return Result<std::vector<uint64_t>>::Error("Truncated trace: " + path.string(), "read_failed");
        }
        hashes.push_back(hash);
    }

    return Result<std::vector<uint64_t>>::Ok(std::move(hashes));
}

Result<std::filesystem::path> ResolveTracePath(const TracePathRequest &request) {
    std::filesystem::path base = request.projectDirectory.empty() ? request.tasRoot : request.projectDirectory;
    if (base.empty()) {
        return Result<std::filesystem::path>::Error("TAS root is not available", "missing_tas_root");
    }

    std::filesystem::path resolved;
    if (request.requestedPath && !request.requestedPath->empty()) {
        resolved = request.requestedPath->is_absolute() ? *request.requestedPath : base / *request.requestedPath;
    } else {
        const std::string stem = SanitizeFileStem(request.projectName.empty() ? "trace" : request.projectName);
        resolved = base / "determinism" / (stem + ".btd");
    }

    std::error_code ec;
    std::filesystem::create_directories(resolved.parent_path(), ec);
    if (ec) {
        return Result<std::filesystem::path>::Error("Failed to create trace directory: " + ec.message(), "directory_create_failed");
    }

    return Result<std::filesystem::path>::Ok(Normalize(resolved));
}

Result<TraceDiff> OfflineDiff(const std::filesystem::path &left, const std::filesystem::path &right) {
    auto leftResult = ReadHashes(left);
    if (leftResult.IsError()) {
        return Result<TraceDiff>::Error(leftResult.GetError().message, leftResult.GetError().category);
    }
    auto rightResult = ReadHashes(right);
    if (rightResult.IsError()) {
        return Result<TraceDiff>::Error(rightResult.GetError().message, rightResult.GetError().category);
    }

    const auto leftHashes = leftResult.Unwrap();
    const auto rightHashes = rightResult.Unwrap();
    const size_t compared = std::min(leftHashes.size(), rightHashes.size());

    TraceDiff diff;
    diff.comparedTicks = compared;
    diff.leftTicks = leftHashes.size();
    diff.rightTicks = rightHashes.size();
    diff.firstDivergenceTick = compared;

    for (size_t i = 0; i < compared; ++i) {
        if (leftHashes[i] != rightHashes[i]) {
            if (diff.firstDivergenceTick == compared) {
                diff.firstDivergenceTick = i;
            }
            ++diff.divergentTicks;
        }
    }

    diff.identical = diff.divergentTicks == 0 && leftHashes.size() == rightHashes.size();
    return Result<TraceDiff>::Ok(diff);
}

} // namespace tas::determinism
