#pragma once

#include "ILogSink.h"
#include <cstdio>
#include <cstdarg>

/**
 * @brief Simple console-based ILogSink for standalone tests.
 *
 * Writes log output to stdout/stderr so tests can run without
 * BML or any game SDK.
 */
class ConsoleLogSink final : public ILogSink {
public:
    void Info(const char *fmt, ...) override {
        va_list args;
        va_start(args, fmt);
        std::printf("[INFO] ");
        std::vprintf(fmt, args);
        std::printf("\n");
        va_end(args);
    }

    void Warn(const char *fmt, ...) override {
        va_list args;
        va_start(args, fmt);
        std::fprintf(stderr, "[WARN] ");
        std::vfprintf(stderr, fmt, args);
        std::fprintf(stderr, "\n");
        va_end(args);
    }

    void Error(const char *fmt, ...) override {
        va_list args;
        va_start(args, fmt);
        std::fprintf(stderr, "[ERROR] ");
        std::vfprintf(stderr, fmt, args);
        std::fprintf(stderr, "\n");
        va_end(args);
    }
};
