#pragma once

class ILogger;

/**
 * @brief Thread-safe global logger namespace
 *
 * Provides a clean, decoupled interface for logging throughout the application.
 * All functions are thread-safe and can be called from any context.
 *
 * Usage:
 *   Log::Info("Player position: %f, %f, %f", x, y, z);
 *   Log::Warn("High frame time detected: %d ms", frameTime);
 *   Log::Error("Failed to load project: %s", projectName.c_str());
 */
namespace Log {
    /**
     * @brief Initialize the logger system
     * @param logger Pointer to ILogger instance
     * @note Must be called before any logging functions
     */
    void Initialize(ILogger *logger);

    /**
     * @brief Shutdown the logger system
     * @note After shutdown, logging calls will be silently ignored
     */
    void Shutdown();

    /**
     * @brief Log an informational message
     * @param fmt printf-style format string
     * @param ... Variable arguments for formatting
     */
    void Info(const char *fmt, ...);

    /**
     * @brief Log a warning message
     * @param fmt printf-style format string
     * @param ... Variable arguments for formatting
     */
    void Warn(const char *fmt, ...);

    /**
     * @brief Log an error message
     * @param fmt printf-style format string
     * @param ... Variable arguments for formatting
     */
    void Error(const char *fmt, ...);
}
