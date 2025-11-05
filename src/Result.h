#pragma once

#include <optional>
#include <string>
#include <stdexcept>
#include <type_traits>
#include <functional>
#include <source_location>
#include <unordered_map>

// ============================================================================
// Error Severity Levels
// ============================================================================

/**
 * @brief Enumeration of error severity levels
 *
 * Used to categorize errors by their importance and impact on system operation.
 */
enum class ErrorSeverity {
    Debug,   // Debug information, lowest priority
    Info,    // Informational message
    Warning, // Warning that doesn't prevent operation
    Error,   // Error that affects functionality
    Critical // Critical error that may cause system failure
};

/**
 * @brief Convert ErrorSeverity to string representation
 * @param severity The severity level to convert
 * @return String representation of the severity level
 */
inline const char *SeverityToString(ErrorSeverity severity) {
    switch (severity) {
    case ErrorSeverity::Debug: return "DEBUG";
    case ErrorSeverity::Info: return "INFO";
    case ErrorSeverity::Warning: return "WARNING";
    case ErrorSeverity::Error: return "ERROR";
    case ErrorSeverity::Critical: return "CRITICAL";
    default: return "UNKNOWN";
    }
}

// ============================================================================
// Error Information Structure
// ============================================================================

/**
 * @brief Detailed error information structure
 *
 * Provides comprehensive error information including message, category,
 * severity level, source location, and additional context data.
 */
struct ErrorInfo {
    std::string message; // Error message
    std::string category = "general"; // Error category (e.g., "validation", "permission")
    ErrorSeverity severity = ErrorSeverity::Error; // Severity level
    std::source_location location = std::source_location::current(); // Source code location
    std::unordered_map<std::string, std::string> context; // Additional context information

    /**
     * @brief Format error information as human-readable string
     * @return Formatted error message with all details
     */
    std::string Format() const {
        std::string result;
        result += "[" + std::string(SeverityToString(severity)) + "] ";
        result += category + ": " + message;
        result += "\n  at " + std::string(location.file_name()) + ":" + std::to_string(location.line());

        if (!context.empty()) {
            result += "\n  Context:";
            for (const auto &[key, value] : context) {
                result += "\n    " + key + ": " + value;
            }
        }

        return result;
    }

    // Constructors
    ErrorInfo() = default;

    /**
     * @brief Construct error information with details
     * @param msg Error message
     * @param cat Error category
     * @param sev Severity level
     * @param loc Source location (automatically captured)
     */
    explicit ErrorInfo(std::string msg,
                       std::string cat = "general",
                       ErrorSeverity sev = ErrorSeverity::Error,
                       std::source_location loc = std::source_location::current())
        : message(std::move(msg)), category(std::move(cat)), severity(sev), location(loc) {
    }

    /**
     * @brief Add context information to the error
     * @param key Context key
     * @param value Context value
     * @return Reference to this ErrorInfo for method chaining
     */
    ErrorInfo &WithContext(const std::string &key, const std::string &value) {
        context[key] = value;
        return *this;
    }
};

// ============================================================================
// Result Type - Rust-style error handling
// ============================================================================

/**
 * @brief Result type for type-safe error handling
 *
 * Represents either a successful value (Ok) or an error (Error). Inspired by
 * Rust's Result<T, E> type, providing compile-time type safety and expressive
 * error handling through method chaining.
 *
 * @tparam T The type of the success value
 *
 * Example usage:
 * @code
 * Result<int> divide(int a, int b) {
 *     if (b == 0) {
 *         return Result<int>::Error("Division by zero");
 *     }
 *     return Result<int>::Ok(a / b);
 * }
 *
 * auto result = divide(10, 2)
 *     .AndThen([](int value) {
 *         return Result<int>::Ok(value * 2);
 *     })
 *     .UnwrapOr(0);
 * @endcode
 */
template <typename T>
class Result {
public:
    // ========================================================================
    // Factory Methods
    // ========================================================================

    /**
     * @brief Create a successful result
     * @param value The success value
     * @return Result containing the value
     */
    static Result<T> Ok(T value) {
        return Result<T>(std::move(value));
    }

    /**
     * @brief Create an error result from ErrorInfo
     * @param error The error information
     * @return Result containing the error
     */
    static Result<T> Error(ErrorInfo error) {
        return Result<T>(std::move(error));
    }

    /**
     * @brief Create an error result with message and details
     * @param message Error message
     * @param category Error category (default: "general")
     * @param severity Error severity (default: Error)
     * @param location Source location (automatically captured)
     * @return Result containing the error
     */
    static Result<T> Error(std::string message,
                           std::string category = "general",
                           ErrorSeverity severity = ErrorSeverity::Error,
                           std::source_location location = std::source_location::current()) {
        return Result<T>(ErrorInfo(std::move(message), std::move(category), severity, location));
    }

    // ========================================================================
    // State Inspection
    // ========================================================================

    /**
     * @brief Check if result contains a success value
     * @return true if Ok, false if Error
     */
    bool IsOk() const { return m_Value.has_value(); }

    /**
     * @brief Check if result contains an error
     * @return true if Error, false if Ok
     */
    bool IsError() const { return m_Error.has_value(); }

    /**
     * @brief Explicit conversion to bool (true if Ok)
     */
    explicit operator bool() const { return IsOk(); }

    // ========================================================================
    // Value Extraction
    // ========================================================================

    /**
     * @brief Unwrap the contained value
     * @return Reference to the contained value
     * @throws std::runtime_error if result is Error
     */
    T &Unwrap() {
        if (!IsOk()) {
            throw std::runtime_error("Unwrap called on Error: " + m_Error->Format());
        }
        return *m_Value;
    }

    /**
     * @brief Unwrap the contained value (const version)
     * @return Const reference to the contained value
     * @throws std::runtime_error if result is Error
     */
    const T &Unwrap() const {
        if (!IsOk()) {
            throw std::runtime_error("Unwrap called on Error: " + m_Error->Format());
        }
        return *m_Value;
    }

    /**
     * @brief Unwrap or return default value
     * @param defaultValue Value to return if Error
     * @return The contained value if Ok, otherwise defaultValue
     */
    T UnwrapOr(T defaultValue) const {
        return IsOk() ? *m_Value : std::move(defaultValue);
    }

    /**
     * @brief Unwrap or compute default value using function
     * @param func Function to compute default value
     * @return The contained value if Ok, otherwise result of func()
     */
    template <typename F>
    T UnwrapOrElse(F &&func) const {
        return IsOk() ? *m_Value : func();
    }

    /**
     * @brief Get error information
     * @return Const reference to ErrorInfo
     * @throws std::logic_error if result is Ok
     */
    const ErrorInfo &GetError() const {
        if (!IsError()) {
            throw std::logic_error("GetError called on Ok");
        }
        return *m_Error;
    }

    // ========================================================================
    // Combinators - Method Chaining
    // ========================================================================

    /**
     * @brief Chain operations on success value
     *
     * If result is Ok, applies the function to the contained value.
     * If result is Error, propagates the error without calling the function.
     *
     * @tparam F Function type: T -> Result<U>
     * @param func Function to apply to success value
     * @return Result<U> from applying func, or propagated error
     */
    template <typename F>
    auto AndThen(F &&func) -> decltype(func(std::declval<T>())) {
        using ResultType = decltype(func(std::declval<T>()));

        if (IsError()) {
            return ResultType::Error(*m_Error);
        }

        try {
            return func(*m_Value);
        } catch (const std::exception &e) {
            return ResultType::Error(
                ErrorInfo(e.what(), "exception", ErrorSeverity::Error)
            );
        }
    }

    /**
     * @brief Recover from error
     *
     * If result is Error, applies the recovery function.
     * If result is Ok, returns self without calling the function.
     *
     * @tparam F Function type: ErrorInfo -> Result<T>
     * @param func Recovery function
     * @return Result<T> from recovery function, or original Ok
     */
    template <typename F>
    Result<T> OrElse(F &&func) {
        if (IsOk()) {
            return *this;
        }

        try {
            return func(*m_Error);
        } catch (const std::exception &e) {
            return Result<T>::Error(
                ErrorInfo(e.what(), "recovery_failed", ErrorSeverity::Critical)
            );
        }
    }

    /**
     * @brief Transform success value
     *
     * Maps the contained value using the provided function.
     *
     * @tparam F Function type: T -> U
     * @param func Transformation function
     * @return Result<U> with transformed value, or propagated error
     */
    template <typename F>
    auto Map(F &&func) -> Result<decltype(func(std::declval<T>()))> {
        using U = decltype(func(std::declval<T>()));

        if (IsError()) {
            return Result<U>::Error(*m_Error);
        }

        try {
            return Result<U>::Ok(func(*m_Value));
        } catch (const std::exception &e) {
            return Result<U>::Error(
                ErrorInfo(e.what(), "map_failed", ErrorSeverity::Error)
            );
        }
    }

    /**
     * @brief Transform error information
     *
     * Maps the contained error using the provided function.
     *
     * @tparam F Function type: ErrorInfo -> ErrorInfo
     * @param func Error transformation function
     * @return Result<T> with transformed error, or original Ok
     */
    template <typename F>
    Result<T> MapError(F &&func) {
        if (IsOk()) {
            return *this;
        }

        try {
            return Result<T>::Error(func(*m_Error));
        } catch (const std::exception &e) {
            return Result<T>::Error(
                ErrorInfo(e.what(), "map_error_failed", ErrorSeverity::Critical)
            );
        }
    }

    // ========================================================================
    // Copy and Move Semantics
    // ========================================================================

    Result(const Result &other) = default;
    Result(Result &&other) noexcept = default;
    Result &operator=(const Result &other) = default;
    Result &operator=(Result &&other) noexcept = default;

private:
    // Private constructors - use factory methods
    explicit Result(T value) : m_Value(std::move(value)) {}

    explicit Result(ErrorInfo error) : m_Error(std::move(error)) {}

    std::optional<T> m_Value;         // Contains value if Ok
    std::optional<ErrorInfo> m_Error; // Contains error if Error
};

// ============================================================================
// Result<void> Specialization
// ============================================================================

/**
 * @brief Result specialization for void return type
 *
 * Represents success without a value, or an error. Used for operations
 * that don't return a meaningful value but can fail.
 *
 * Example:
 * @code
 * Result<void> initialize() {
 *     if (!setupSucceeded) {
 *         return Result<void>::Error("Setup failed");
 *     }
 *     return Result<void>::Ok();
 * }
 * @endcode
 */
template <>
class Result<void> {
public:
    // ========================================================================
    // Factory Methods
    // ========================================================================

    /**
     * @brief Create a successful void result
     * @return Result representing success
     */
    static Result<void> Ok() {
        return Result<void>(true);
    }

    /**
     * @brief Create an error result from ErrorInfo
     * @param error The error information
     * @return Result containing the error
     */
    static Result<void> Error(ErrorInfo error) {
        return Result<void>(std::move(error));
    }

    /**
     * @brief Create an error result with message and details
     * @param message Error message
     * @param category Error category (default: "general")
     * @param severity Error severity (default: Error)
     * @param location Source location (automatically captured)
     * @return Result containing the error
     */
    static Result<void> Error(std::string message,
                              std::string category = "general",
                              ErrorSeverity severity = ErrorSeverity::Error,
                              std::source_location location = std::source_location::current()) {
        return Result<void>(ErrorInfo(std::move(message), std::move(category), severity, location));
    }

    // ========================================================================
    // State Inspection
    // ========================================================================

    bool IsOk() const { return m_IsOk; }
    bool IsError() const { return !m_IsOk; }
    explicit operator bool() const { return IsOk(); }

    // ========================================================================
    // Value Extraction
    // ========================================================================

    /**
     * @brief Check if operation succeeded
     * @throws std::runtime_error if result is Error
     */
    void Unwrap() const {
        if (!IsOk()) {
            throw std::runtime_error("Unwrap called on Error: " + m_Error->Format());
        }
    }

    /**
     * @brief Get error information
     * @return Const reference to ErrorInfo
     * @throws std::logic_error if result is Ok
     */
    const ErrorInfo &GetError() const {
        if (!IsError()) {
            throw std::logic_error("GetError called on Ok");
        }
        return *m_Error;
    }

    // ========================================================================
    // Combinators
    // ========================================================================

    /**
     * @brief Chain operations on success
     * @tparam F Function type: void -> Result<U>
     * @param func Function to execute if Ok
     * @return Result from func, or propagated error
     */
    template <typename F>
    auto AndThen(F &&func) -> decltype(func()) {
        if (IsError()) {
            using RetType = decltype(func());
            return RetType::Error(*m_Error);
        }

        try {
            return func();
        } catch (const std::exception &e) {
            using RetType = decltype(func());
            return RetType::Error(
                ErrorInfo(e.what(), "exception", ErrorSeverity::Error)
            );
        }
    }

    /**
     * @brief Recover from error
     * @tparam F Function type: ErrorInfo -> Result<void>
     * @param func Recovery function
     * @return Result from recovery, or original Ok
     */
    template <typename F>
    Result<void> OrElse(F &&func) {
        if (IsOk()) {
            return *this;
        }

        try {
            return func(*m_Error);
        } catch (const std::exception &e) {
            return Result<void>::Error(
                ErrorInfo(e.what(), "recovery_failed", ErrorSeverity::Critical)
            );
        }
    }

    // ========================================================================
    // Copy and Move Semantics
    // ========================================================================

    Result(const Result &other) = default;
    Result(Result &&other) noexcept = default;
    Result &operator=(const Result &other) = default;
    Result &operator=(Result &&other) noexcept = default;

private:
    explicit Result(bool isOk) : m_IsOk(isOk) {}

    explicit Result(ErrorInfo error) : m_IsOk(false), m_Error(std::move(error)) {}

    bool m_IsOk = false;
    std::optional<ErrorInfo> m_Error;
};

// ============================================================================
// Convenience Macros
// ============================================================================

/**
 * @brief Create a successful Result with the given value
 * Usage: TAS_OK(42)
 */
#define TAS_OK(value) Result<decltype(value)>::Ok(value)

/**
 * @brief Create a successful Result<void>
 * Usage: TAS_OK_VOID()
 */
#define TAS_OK_VOID() Result<void>::Ok()

/**
 * @brief Create an error Result with message
 * Usage: TAS_ERROR(int, "Invalid value")
 */
#define TAS_ERROR(type, msg) Result<type>::Error(msg, "general", ErrorSeverity::Error)

/**
 * @brief Create an error Result<void> with message
 * Usage: TAS_ERROR_VOID("Operation failed")
 */
#define TAS_ERROR_VOID(msg) Result<void>::Error(msg, "general", ErrorSeverity::Error)

/**
 * @brief Early return on error (try operator)
 *
 * Evaluates expression and returns error if it contains one.
 * Otherwise, unwraps and continues.
 *
 * Usage:
 * @code
 * Result<void> doWork() {
 *     int value = TAS_TRY(parseValue());
 *     // Continue with value...
 *     return Result<void>::Ok();
 * }
 * @endcode
 */
#define TAS_TRY(expr) \
    ({ \
        auto __result = (expr); \
        if (!__result.IsOk()) { \
            return decltype(__result)::Error(__result.GetError()); \
        } \
        __result.Unwrap(); \
    })
