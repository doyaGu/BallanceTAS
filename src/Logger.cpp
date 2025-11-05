#include "Logger.h"
#include <BML/ILogger.h>
#include <mutex>
#include <cstdarg>
#include <cstdio>

namespace {
    ILogger *g_Logger = nullptr;
    std::mutex g_LoggerMutex;

    constexpr size_t MAX_LOG_MESSAGE_SIZE = 4096;
}

namespace Log {
    void Initialize(ILogger *logger) {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        g_Logger = logger;
    }

    void Shutdown() {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        g_Logger = nullptr;
    }

    void Info(const char *fmt, ...) {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        if (!g_Logger) return;

        char buffer[MAX_LOG_MESSAGE_SIZE];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        g_Logger->Info("%s", buffer);
    }

    void Warn(const char *fmt, ...) {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        if (!g_Logger) return;

        char buffer[MAX_LOG_MESSAGE_SIZE];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        g_Logger->Warn("%s", buffer);
    }

    void Error(const char *fmt, ...) {
        std::lock_guard<std::mutex> lock(g_LoggerMutex);
        if (!g_Logger) return;

        char buffer[MAX_LOG_MESSAGE_SIZE];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        g_Logger->Error("%s", buffer);
    }
}
