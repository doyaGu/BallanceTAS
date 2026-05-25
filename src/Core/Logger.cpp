#include "Logger.h"
#include "ILogSink.h"
#include <mutex>
#include <cstdarg>
#include <cstdio>

static ILogSink *g_Sink = nullptr;
static std::mutex g_LoggerMutex;

constexpr size_t MAX_LOG_MESSAGE_SIZE = 4096;

namespace Log {
    void Initialize(ILogSink *sink) {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        g_Sink = sink;
    }

    void Shutdown() {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        g_Sink = nullptr;
    }

    void Info(const char *fmt, ...) {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        if (!g_Sink) return;

        char buffer[MAX_LOG_MESSAGE_SIZE];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        g_Sink->Info("%s", buffer);
    }

    void Warn(const char *fmt, ...) {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        if (!g_Sink) return;

        char buffer[MAX_LOG_MESSAGE_SIZE];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        g_Sink->Warn("%s", buffer);
    }

    void Error(const char *fmt, ...) {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        if (!g_Sink) return;

        char buffer[MAX_LOG_MESSAGE_SIZE];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        g_Sink->Error("%s", buffer);
    }
}
