#include <gtest/gtest.h>

#include "Workflow/HarnessProtocol.h"

TEST(HarnessProtocolTest, ParsesVersionedRequest) {
    HarnessRequest request;
    std::string error;
    ASSERT_TRUE(ParseHarnessRequest(
        R"({"protocol":1,"project":"LuaRuntimeSmoke","max_ticks":4321})", request, error));
    EXPECT_EQ(request.protocol, 1U);
    EXPECT_EQ(request.project, "LuaRuntimeSmoke");
    EXPECT_EQ(request.maxTicks, 4321U);
}

TEST(HarnessProtocolTest, RejectsInvalidContract) {
    HarnessRequest request;
    std::string error;
    EXPECT_FALSE(ParseHarnessRequest(R"({"protocol":2,"project":"Smoke"})", request, error));
    EXPECT_FALSE(error.empty());

    error.clear();
    EXPECT_FALSE(ParseHarnessRequest(R"({"protocol":1,"project":""})", request, error));
    EXPECT_FALSE(error.empty());
}

TEST(HarnessProtocolTest, BuildsPlayerCompatibleResult) {
    const std::string result = BuildHarnessResult(true, "Smoke", "all checks passed", 42);
    EXPECT_NE(result.find(R"("status": "passed")"), std::string::npos);
    EXPECT_NE(result.find(R"("exit_code": 0)"), std::string::npos);
    EXPECT_NE(result.find(R"("tick": 42)"), std::string::npos);
}
