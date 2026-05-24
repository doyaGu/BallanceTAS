#include <gtest/gtest.h>

// The old in-process Lua API integration test depended on a partial mock of
// the game engine and no longer matched the runtime architecture. Lua API
// compatibility is currently verified by the real Ballance smoke project under
// ModLoader/TAS/LuaRuntimeSmoke, plus focused LuaRuntimeTest coverage.
TEST(LuaApiTest, RealGameSmokeOwnsApiCompatibility) {
    SUCCEED();
}
