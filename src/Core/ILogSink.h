#pragma once

#include <cstdarg>

/**
 * @brief Pure C++ logging interface — no BML / SDK dependency.
 *
 * Implementations can forward to BML's ILogger, to stdout, or to a
 * test-harness capture buffer.  The interface mirrors BML::ILogger on
 * purpose so that the adapter is trivial.
 */
class ILogSink {
public:
    virtual ~ILogSink() = default;

    virtual void Info(const char *fmt, ...) = 0;
    virtual void Warn(const char *fmt, ...) = 0;
    virtual void Error(const char *fmt, ...) = 0;
};
