#include <gtest/gtest.h>

#include "Workflow/ValidationService.h"

#include <cmath>
#include <limits>

namespace {

FrameData MakeFrame(size_t frame,
                    const VxVector &position = VxVector(0.0f, 0.0f, 0.0f),
                    const VxVector &velocity = VxVector(0.0f, 0.0f, 0.0f)) {
    FrameData data;
    data.frameIndex = frame;
    data.physics.position = position;
    data.physics.velocity = velocity;
    return data;
}

TEST(ValidationServiceTest, IdenticalWithinTolerance) {
    const std::vector<FrameData> reference{
        MakeFrame(10),
        MakeFrame(11, VxVector(1.0f, 2.0f, 3.0f), VxVector(4.0f, 5.0f, 6.0f)),
    };
    const std::vector<FrameData> live{
        MakeFrame(10),
        MakeFrame(11, VxVector(1.5f, 2.0f, 3.0f), VxVector(4.0f, 6.0f, 6.0f)),
    };

    const auto report = ValidationService::CompareFrameData(live, reference, {1.0f, 2.0f});

    EXPECT_TRUE(report.hasReference);
    EXPECT_TRUE(report.identical);
    EXPECT_EQ(report.comparedFrames, 2u);
    EXPECT_EQ(report.divergentFrames, 0u);
    EXPECT_FLOAT_EQ(report.maxPositionError, 0.5f);
    EXPECT_EQ(report.maxPositionFrame, 11u);
    EXPECT_FLOAT_EQ(report.maxVelocityError, 1.0f);
    EXPECT_EQ(report.maxVelocityFrame, 11u);
}

TEST(ValidationServiceTest, ReportsFirstAndWorstDivergence) {
    const std::vector<FrameData> reference{
        MakeFrame(20), MakeFrame(21), MakeFrame(22),
    };
    const std::vector<FrameData> live{
        MakeFrame(20),
        MakeFrame(21, VxVector(2.0f, 0.0f, 0.0f)),
        MakeFrame(22, VxVector(), VxVector(0.0f, 3.0f, 0.0f)),
    };

    const auto report = ValidationService::CompareFrameData(live, reference, {1.0f, 2.0f});

    EXPECT_FALSE(report.identical);
    EXPECT_EQ(report.divergentFrames, 2u);
    EXPECT_EQ(report.firstDivergenceFrame, 21u);
    EXPECT_FLOAT_EQ(report.maxPositionError, 2.0f);
    EXPECT_EQ(report.maxPositionFrame, 21u);
    EXPECT_FLOAT_EQ(report.maxVelocityError, 3.0f);
    EXPECT_EQ(report.maxVelocityFrame, 22u);
}

TEST(ValidationServiceTest, CountsMissingFramesAsDivergence) {
    const std::vector<FrameData> reference{MakeFrame(100)};
    const std::vector<FrameData> live{MakeFrame(100), MakeFrame(101), MakeFrame(102)};

    const auto report = ValidationService::CompareFrameData(live, reference);

    EXPECT_FALSE(report.identical);
    EXPECT_EQ(report.comparedFrames, 1u);
    EXPECT_EQ(report.divergentFrames, 2u);
    EXPECT_EQ(report.firstDivergenceFrame, 101u);
}

TEST(ValidationServiceTest, FrameIndexMismatchDiverges) {
    const std::vector<FrameData> reference{MakeFrame(8)};
    const std::vector<FrameData> live{MakeFrame(7)};

    const auto report = ValidationService::CompareFrameData(live, reference);

    EXPECT_FALSE(report.identical);
    EXPECT_EQ(report.divergentFrames, 1u);
    EXPECT_EQ(report.firstDivergenceFrame, 7u);
}

TEST(ValidationServiceTest, NonFinitePhysicsCannotMatch) {
    auto invalid = MakeFrame(1);
    invalid.physics.position.x = std::numeric_limits<float>::quiet_NaN();

    const auto report = ValidationService::CompareFrameData({invalid}, {MakeFrame(1)});

    EXPECT_FALSE(report.identical);
    EXPECT_EQ(report.divergentFrames, 1u);
    EXPECT_EQ(report.firstDivergenceFrame, 1u);
    EXPECT_TRUE(std::isinf(report.maxPositionError));
}

} // namespace
