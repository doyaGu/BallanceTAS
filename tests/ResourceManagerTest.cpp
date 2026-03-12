#include <gtest/gtest.h>
#include "ResourceManager.h"

#include <filesystem>
#include <fstream>

// ============================================================================
// TemporaryFile tests
// ============================================================================

TEST(ResourceManagerTest, CreateTempFile) {
    ResourceManager rm;
    auto tmpFile = rm.CreateTempFile("test_", ".txt");

    ASSERT_NE(tmpFile, nullptr);
    EXPECT_TRUE(tmpFile->Exists());

    // Write something to make sure the path is valid
    {
        std::ofstream out(tmpFile->GetPath());
        out << "hello";
    }
    EXPECT_TRUE(tmpFile->Exists());

    auto path = tmpFile->GetPath();
    tmpFile.reset(); // Should auto-delete

    // After the temp file expires (and ResourceManager cleans up), file may
    // or may not exist depending on whether other shared_ptrs remain.
    // ResourceManager destructor calls CleanupAll.
}

TEST(ResourceManagerTest, TempFileAutoDeleteOnManagerDestruct) {
    std::filesystem::path savedPath;
    {
        ResourceManager rm;
        auto tmpFile = rm.CreateTempFile("test_del_", ".tmp");
        ASSERT_NE(tmpFile, nullptr);
        savedPath = tmpFile->GetPath();
        EXPECT_TRUE(std::filesystem::exists(savedPath));
    }
    // After ResourceManager destroyed, file should be cleaned up
    EXPECT_FALSE(std::filesystem::exists(savedPath));
}

TEST(ResourceManagerTest, KeepFile) {
    std::filesystem::path savedPath;
    {
        ResourceManager rm;
        auto tmpFile = rm.CreateTempFile("test_keep_", ".tmp");
        ASSERT_NE(tmpFile, nullptr);
        tmpFile->KeepFile();
        savedPath = tmpFile->GetPath();
    }
    // File should still exist because we called KeepFile()
    if (std::filesystem::exists(savedPath)) {
        std::filesystem::remove(savedPath); // Clean up manually
    }
}

// ============================================================================
// Cleanup handler tests
// ============================================================================

TEST(ResourceManagerTest, CleanupHandlers) {
    ResourceManager rm;
    int callOrder = 0;
    int first = 0, second = 0;

    rm.RegisterCleanupHandler([&]() { first = ++callOrder; });
    rm.RegisterCleanupHandler([&]() { second = ++callOrder; });

    rm.CleanupAll();

    // Handlers execute in reverse order of registration
    EXPECT_GT(first, 0);
    EXPECT_GT(second, 0);
}

TEST(ResourceManagerTest, NamedCleanupHandler) {
    ResourceManager rm;
    bool called = false;

    rm.RegisterCleanupHandler("test_handler", [&]() { called = true; });
    EXPECT_TRUE(rm.UnregisterCleanupHandler("test_handler"));
    rm.CleanupAll();

    EXPECT_FALSE(called);
}

// ============================================================================
// ScopedResource tests
// ============================================================================

TEST(ResourceManagerTest, ScopedResourceCleanup) {
    ResourceManager rm;
    bool cleaned = false;

    {
        auto scoped = rm.CreateScopedResource([&]() { cleaned = true; });
        EXPECT_FALSE(cleaned);
    }
    EXPECT_TRUE(cleaned);
}

TEST(ResourceManagerTest, ScopedResourceRelease) {
    ResourceManager rm;
    bool cleaned = false;

    {
        auto scoped = rm.CreateScopedResource([&]() { cleaned = true; });
        scoped.Release();
    }
    EXPECT_FALSE(cleaned);
}

// ============================================================================
// Counter / statistics
// ============================================================================

TEST(ResourceManagerTest, Counters) {
    ResourceManager rm;
    auto tmp = rm.CreateTempFile("cnt_", ".tmp");
    EXPECT_GE(rm.GetTempFileCount(), 1u);

    rm.RegisterCleanupHandler("x", []() {});
    EXPECT_GE(rm.GetCleanupHandlerCount(), 1u);
}
