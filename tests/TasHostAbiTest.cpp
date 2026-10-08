#include <gtest/gtest.h>

#include "Host/TasHostApi.h"

TEST(TasHostAbiTest, ContractHasStableRequiredOffsets) {
    EXPECT_EQ(BALLANCE_TAS_HOST_ABI_VERSION, 1u);
    EXPECT_EQ(offsetof(BallanceTasHostLaunchSpec, target_utf8), 8u);
    EXPECT_EQ(offsetof(BallanceTasRuntimeCallbacks, query_status), 8u);
    EXPECT_EQ(offsetof(BallanceTasHostApi, get_launch_spec), 8u);
    EXPECT_EQ(BALLANCE_TAS_HOST_UNKNOWN_FRAME_COUNT, UINT64_MAX);
}

TEST(TasHostAbiTest, StatusCanRepresentUnknownLuaLength) {
    BallanceTasRuntimeStatus status{};
    status.size = sizeof(status);
    status.abi_version = BALLANCE_TAS_HOST_ABI_VERSION;
    status.phase = BALLANCE_TAS_RUNTIME_ARMED;
    status.project_type = BALLANCE_TAS_PROJECT_LUA;
    status.total_frames = BALLANCE_TAS_HOST_UNKNOWN_FRAME_COUNT;

    EXPECT_EQ(status.phase, BALLANCE_TAS_RUNTIME_ARMED);
    EXPECT_EQ(status.project_type, BALLANCE_TAS_PROJECT_LUA);
    EXPECT_EQ(status.total_frames, UINT64_MAX);
}
