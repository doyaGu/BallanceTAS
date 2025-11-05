#include <gtest/gtest.h>
#include "Result.h"
#include <string>

// ============================================================================
// Result<T> Basic Tests
// ============================================================================

TEST(ResultTest, Ok) {
    auto result = Result<int>::Ok(42);

    ASSERT_TRUE(result.IsOk());
    ASSERT_FALSE(result.IsError());
    ASSERT_EQ(result.Unwrap(), 42);
}

TEST(ResultTest, Error) {
    auto result = Result<int>::Error("Something went wrong");

    ASSERT_FALSE(result.IsOk());
    ASSERT_TRUE(result.IsError());
    ASSERT_EQ(result.GetError().message, "Something went wrong");
}

TEST(ResultTest, UnwrapOr) {
    auto okResult = Result<int>::Ok(42);
    auto errorResult = Result<int>::Error("Error");

    ASSERT_EQ(okResult.UnwrapOr(100), 42);
    ASSERT_EQ(errorResult.UnwrapOr(100), 100);
}

TEST(ResultTest, UnwrapOrElse) {
    auto okResult = Result<int>::Ok(42);
    auto errorResult = Result<int>::Error("Error");

    ASSERT_EQ(okResult.UnwrapOrElse([]() { return 100; }), 42);
    ASSERT_EQ(errorResult.UnwrapOrElse([]() { return 100; }), 100);
}

// ============================================================================
// Result<T> Chaining Tests
// ============================================================================

TEST(ResultTest, AndThen) {
    auto divide = [](int a, int b) -> Result<int> {
        if (b == 0) {
            return Result<int>::Error("Division by zero");
        }
        return Result<int>::Ok(a / b);
    };

    auto result1 = divide(10, 2).AndThen([](int value) -> Result<int> {
        return Result<int>::Ok(value * 2);
    });
    ASSERT_TRUE(result1.IsOk());
    ASSERT_EQ(result1.Unwrap(), 10);

    auto result2 = divide(10, 0).AndThen([](int value) -> Result<int> {
        return Result<int>::Ok(value * 2);
    });
    ASSERT_TRUE(result2.IsError());
}

TEST(ResultTest, OrElse) {
    auto errorResult = Result<int>::Error("Initial error");

    auto recovered = errorResult.OrElse([](const ErrorInfo& error) {
        return Result<int>::Ok(42);  // Recover to default value
    });

    ASSERT_TRUE(recovered.IsOk());
    ASSERT_EQ(recovered.Unwrap(), 42);
}

TEST(ResultTest, Map) {
    auto okResult = Result<int>::Ok(5);

    auto mapped = okResult.Map([](int value) {
        return value * 2;
    });

    ASSERT_TRUE(mapped.IsOk());
    ASSERT_EQ(mapped.Unwrap(), 10);

    auto errorResult = Result<int>::Error("Error");
    auto mappedError = errorResult.Map([](int value) {
        return value * 2;
    });

    ASSERT_TRUE(mappedError.IsError());
}

TEST(ResultTest, MapError) {
    auto errorResult = Result<int>::Error("Original error");

    auto mappedError = errorResult.MapError([](const ErrorInfo& error) {
        ErrorInfo newError = error;
        newError.message = "Mapped: " + error.message;
        return newError;
    });

    ASSERT_TRUE(mappedError.IsError());
    ASSERT_EQ(mappedError.GetError().message, "Mapped: Original error");
}

// ============================================================================
// Result<void> Tests
// ============================================================================

TEST(ResultVoidTest, Ok) {
    auto result = Result<void>::Ok();

    ASSERT_TRUE(result.IsOk());
    ASSERT_FALSE(result.IsError());
}

TEST(ResultVoidTest, Error) {
    auto result = Result<void>::Error("Operation failed");

    ASSERT_FALSE(result.IsOk());
    ASSERT_TRUE(result.IsError());
    ASSERT_EQ(result.GetError().message, "Operation failed");
}

TEST(ResultVoidTest, AndThen) {
    int counter = 0;

    auto result = Result<void>::Ok()
        .AndThen([&counter]() {
            counter++;
            return Result<void>::Ok();
        })
        .AndThen([&counter]() {
            counter++;
            return Result<void>::Ok();
        });

    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(counter, 2);
}

TEST(ResultVoidTest, AndThenError) {
    int counter = 0;

    auto result = Result<void>::Ok()
        .AndThen([&counter]() {
            counter++;
            return Result<void>::Error("Failed");
        })
        .AndThen([&counter]() {
            counter++;  // Should not execute
            return Result<void>::Ok();
        });

    ASSERT_TRUE(result.IsError());
    ASSERT_EQ(counter, 1);  // Second AndThen should not execute
}

// ============================================================================
// ErrorInfo Tests
// ============================================================================

TEST(ErrorInfoTest, Construction) {
    ErrorInfo error("Test error", "validation", ErrorSeverity::Warning);
    error.WithContext("key1", "value1");
    error.WithContext("key2", "value2");

    ASSERT_EQ(error.message, "Test error");
    ASSERT_EQ(error.category, "validation");
    ASSERT_EQ(error.severity, ErrorSeverity::Warning);
    ASSERT_EQ(error.context.size(), 2);
    ASSERT_EQ(error.context["key1"], "value1");

    std::string formatted = error.Format();
    ASSERT_TRUE(formatted.find("Test error") != std::string::npos);
    ASSERT_TRUE(formatted.find("validation") != std::string::npos);
}

// ============================================================================
// Complex Scenario Tests
// ============================================================================

Result<int> ParseInt(const std::string& str) {
    try {
        int value = std::stoi(str);
        return Result<int>::Ok(value);
    } catch (...) {
        return Result<int>::Error("Invalid integer: " + str, "parsing");
    }
}

Result<int> ValidatePositive(int value) {
    if (value > 0) {
        return Result<int>::Ok(value);
    }
    return Result<int>::Error("Value must be positive", "validation");
}

Result<int> CalculateSquare(int value) {
    return Result<int>::Ok(value * value);
}

TEST(ComplexScenarioTest, Chaining) {
    // Success chain
    auto result1 = ParseInt("5")
        .AndThen([](int v) { return ValidatePositive(v); })
        .AndThen([](int v) { return CalculateSquare(v); });

    ASSERT_TRUE(result1.IsOk());
    ASSERT_EQ(result1.Unwrap(), 25);

    // Parse failure
    auto result2 = ParseInt("abc")
        .AndThen([](int v) { return ValidatePositive(v); })
        .AndThen([](int v) { return CalculateSquare(v); });

    ASSERT_TRUE(result2.IsError());
    ASSERT_EQ(result2.GetError().category, "parsing");

    // Validation failure
    auto result3 = ParseInt("-5")
        .AndThen([](int v) { return ValidatePositive(v); })
        .AndThen([](int v) { return CalculateSquare(v); });

    ASSERT_TRUE(result3.IsError());
    ASSERT_EQ(result3.GetError().category, "validation");
}

TEST(ComplexScenarioTest, ErrorRecovery) {
    auto result = ParseInt("invalid")
        .OrElse([](const ErrorInfo& error) {
            // Attempt recovery
            return Result<int>::Ok(0);  // Default value
        })
        .AndThen([](int v) { return CalculateSquare(v); });

    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(result.Unwrap(), 0);
}

