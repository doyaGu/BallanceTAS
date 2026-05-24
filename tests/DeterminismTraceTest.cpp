#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "DeterminismTrace.h"

constexpr uint32_t kMagic = 0x53415442;
constexpr uint32_t kVersion = 1;

static void WriteTrace(const std::filesystem::path &path, const std::vector<uint64_t> &hashes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    const uint32_t flags = 0;
    const uint64_t tickCount = hashes.size();
    const uint16_t levelNameLength = 0;
    file.write(reinterpret_cast<const char *>(&kMagic), sizeof(kMagic));
    file.write(reinterpret_cast<const char *>(&kVersion), sizeof(kVersion));
    file.write(reinterpret_cast<const char *>(&flags), sizeof(flags));
    file.write(reinterpret_cast<const char *>(&tickCount), sizeof(tickCount));
    file.write(reinterpret_cast<const char *>(&levelNameLength), sizeof(levelNameLength));
    for (size_t i = 0; i < hashes.size(); ++i) {
        const uint64_t tick = i;
        file.write(reinterpret_cast<const char *>(&tick), sizeof(tick));
        file.write(reinterpret_cast<const char *>(&hashes[i]), sizeof(hashes[i]));
    }
}

TEST(DeterminismTraceTest, ResolvesDefaultPathInsideCurrentProject) {
    tas::determinism::TracePathRequest request;
    request.projectDirectory = "C:/Ballance/ModLoader/TAS/MyProject";
    request.projectName = "My Project";
    request.tasRoot = "C:/Ballance/ModLoader/TAS";

    auto result = tas::determinism::ResolveTracePath(request);

    ASSERT_TRUE(result.IsOk()) << result.GetError().Format();
    EXPECT_EQ(result.Unwrap().generic_string(),
              "C:/Ballance/ModLoader/TAS/MyProject/determinism/My_Project.btd");
}

TEST(DeterminismTraceTest, ResolvesRelativePathInsideTasRootWithoutProject) {
    tas::determinism::TracePathRequest request;
    request.requestedPath = "runs/check.btd";
    request.tasRoot = "C:/Ballance/ModLoader/TAS";

    auto result = tas::determinism::ResolveTracePath(request);

    ASSERT_TRUE(result.IsOk()) << result.GetError().Format();
    EXPECT_EQ(result.Unwrap().generic_string(), "C:/Ballance/ModLoader/TAS/runs/check.btd");
}

TEST(DeterminismTraceTest, OfflineDiffReportsFirstDivergenceTick) {
    auto dir = std::filesystem::temp_directory_path() / "BallanceTAS_DeterminismTrace";
    auto left = dir / "left.btd";
    auto right = dir / "right.btd";
    WriteTrace(left, {10, 20, 30, 40});
    WriteTrace(right, {10, 20, 99, 40, 50});

    auto result = tas::determinism::OfflineDiff(left, right);

    ASSERT_TRUE(result.IsOk()) << result.GetError().Format();
    const auto diff = result.Unwrap();
    EXPECT_FALSE(diff.identical);
    EXPECT_EQ(diff.firstDivergenceTick, 2u);
    EXPECT_EQ(diff.divergentTicks, 1u);
    EXPECT_EQ(diff.comparedTicks, 4u);
    EXPECT_EQ(diff.leftTicks, 4u);
    EXPECT_EQ(diff.rightTicks, 5u);
}
