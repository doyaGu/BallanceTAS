#pragma once

#include <cstddef>
#include <cstdint>

namespace TASConstants {
    inline constexpr const char *DefaultBasePath = "..\\ModLoader\\TAS\\";
    inline constexpr uint16_t DefaultREPLPort = 7878;
    inline constexpr int DefaultSleepInterval = 8;
    inline constexpr size_t DefaultCustomContextMemoryLimit = 10 * 1024 * 1024;
    inline constexpr size_t DefaultMaxPoolSize = 4;
}
