#include <gtest/gtest.h>
#include "Logger.h"
#include "ILogSink.h"
#include "ConsoleLogSink.h"

#include <string>
#include <vector>
#include <mutex>
#include <cstdarg>
#include <cstdio>

// ============================================================================
// Capture sink – records every message for assertions
// ============================================================================

struct LogEntry {
    std::string level;
    std::string message;
};

class CapturingLogSink final : public ILogSink {
public:
    void Info(const char *fmt, ...) override {
        char buf[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        std::lock_guard<std::mutex> lk(m);
        entries.push_back({"INFO", buf});
    }

    void Warn(const char *fmt, ...) override {
        char buf[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        std::lock_guard<std::mutex> lk(m);
        entries.push_back({"WARN", buf});
    }

    void Error(const char *fmt, ...) override {
        char buf[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        std::lock_guard<std::mutex> lk(m);
        entries.push_back({"ERROR", buf});
    }

    std::vector<LogEntry> entries;
    std::mutex m;
};

// ============================================================================
// Tests
// ============================================================================

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        sink = std::make_unique<CapturingLogSink>();
        Log::Initialize(sink.get());
    }

    void TearDown() override {
        Log::Shutdown();
        sink.reset();
    }

    std::unique_ptr<CapturingLogSink> sink;
};

TEST_F(LoggerTest, InfoIsCaptured) {
    Log::Info("hello %s", "world");
    ASSERT_EQ(sink->entries.size(), 1u);
    EXPECT_EQ(sink->entries[0].level, "INFO");
    EXPECT_EQ(sink->entries[0].message, "hello world");
}

TEST_F(LoggerTest, WarnIsCaptured) {
    Log::Warn("caution %d", 42);
    ASSERT_EQ(sink->entries.size(), 1u);
    EXPECT_EQ(sink->entries[0].level, "WARN");
    EXPECT_EQ(sink->entries[0].message, "caution 42");
}

TEST_F(LoggerTest, ErrorIsCaptured) {
    Log::Error("oops %s %d", "code", 500);
    ASSERT_EQ(sink->entries.size(), 1u);
    EXPECT_EQ(sink->entries[0].level, "ERROR");
    EXPECT_EQ(sink->entries[0].message, "oops code 500");
}

TEST_F(LoggerTest, SilentAfterShutdown) {
    Log::Shutdown();
    Log::Info("should not crash");
    // Nothing captured — sink should be detached
    EXPECT_EQ(sink->entries.size(), 0u);
}

TEST_F(LoggerTest, ConsoleLogSinkDoesNotCrash) {
    ConsoleLogSink console;
    Log::Shutdown();
    Log::Initialize(&console);
    Log::Info("console info %d", 1);
    Log::Warn("console warn %s", "!");
    Log::Error("console error");
    Log::Shutdown();
    // No crash = pass
}

TEST_F(LoggerTest, MultipleMessages) {
    Log::Info("a");
    Log::Warn("b");
    Log::Error("c");
    EXPECT_EQ(sink->entries.size(), 3u);
}
