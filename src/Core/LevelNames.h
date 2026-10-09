#pragma once

#include <cctype>
#include <optional>
#include <string>
#include <string_view>

/// Ballance's built-in levels are numbered 1-13 ("Level_01" ... "Level_13").
namespace LevelNames {
    constexpr int kFirstLevel = 1;
    constexpr int kLastLevel = 13;

    inline bool IsBuiltinLevel(int level) {
        return level >= kFirstLevel && level <= kLastLevel;
    }

    /// Accepts "3", "Level_03", "Level 3" or "level3" (case-insensitive).
    inline std::optional<int> Parse(std::string_view text) {
        std::string lowered;
        for (char c : text) {
            if (!std::isspace(static_cast<unsigned char>(c)) && c != '_') {
                lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
        }
        std::string_view digits = lowered;
        if (digits.substr(0, 5) == "level") {
            digits.remove_prefix(5);
        }
        if (digits.empty() || digits.size() > 2) {
            return std::nullopt;
        }
        int level = 0;
        for (char c : digits) {
            if (c < '0' || c > '9') {
                return std::nullopt;
            }
            level = level * 10 + (c - '0');
        }
        if (!IsBuiltinLevel(level)) {
            return std::nullopt;
        }
        return level;
    }

    /// "Level_03" — the key the game and manifests use.
    inline std::string ToKey(int level) {
        return std::string("Level_") + (level < 10 ? "0" : "") + std::to_string(level);
    }
}
