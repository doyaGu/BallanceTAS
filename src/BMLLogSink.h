#pragma once

#include "ILogSink.h"
#include <BML/ILogger.h>
#include <cstdarg>
#include <cstdio>

/**
 * @brief Adapter that forwards ILogSink calls to a BML ILogger.
 *
 * This is the only place in the codebase that bridges the pure-C++ logging
 * abstraction with the BML SDK, keeping tas_core free of SDK headers.
 */
class BMLLogSink final : public ILogSink {
public:
    explicit BMLLogSink(ILogger *bmlLogger) : m_Logger(bmlLogger) {}

    void Info(const char *fmt, ...) override {
        if (!m_Logger) return;
        char buf[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        m_Logger->Info("%s", buf);
    }

    void Warn(const char *fmt, ...) override {
        if (!m_Logger) return;
        char buf[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        m_Logger->Warn("%s", buf);
    }

    void Error(const char *fmt, ...) override {
        if (!m_Logger) return;
        char buf[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        m_Logger->Error("%s", buf);
    }

private:
    ILogger *m_Logger;
};
