#pragma once

#include <string>

namespace tas::context {

inline std::string TrimContextKey(const std::string &value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

inline std::string ResolveLevelKey(const std::string &manifestLevel,
                                   const std::string &mapName,
                                   int levelNumber) {
    std::string key = TrimContextKey(manifestLevel);
    if (!key.empty()) {
        return key;
    }

    key = TrimContextKey(mapName);
    if (!key.empty()) {
        return key;
    }

    if (levelNumber > 0) {
        return "Level_" + std::to_string(levelNumber);
    }

    return {};
}

inline std::string MakeLevelContextName(const std::string &levelKey) {
    return "level:" + TrimContextKey(levelKey);
}

} // namespace tas::context
