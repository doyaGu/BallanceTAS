#include <gtest/gtest.h>

#include "LuaApi/LuaApi.h"

#include "LuaRuntime/LuaProtectedCall.h"
#include "LuaRuntime/LuaState.h"

static void RegisterSharedBuffer(lua_State *L) {
    LuaApi::RegisterSharedBufferApi(L, reinterpret_cast<ScriptContext *>(0x1));
}

static void RunScript(tas::lua::LuaState &state, const char *script, const char *chunkName) {
    auto load = state.LoadString(script, chunkName);
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    auto call = tas::lua::ProtectedCall(state.Get(), 0, 0);
    ASSERT_TRUE(call.IsOk()) << call.GetError().Format();
}

TEST(SharedBufferTest, RoundTripsTableValuesWithoutLosingIntegerNumberDistinction) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    RegisterSharedBuffer(state.Get());

    RunScript(state,
        "local sb = tas.shared_buffer.from_table({ count = 3, precise = 1.25, nested = { ok = true }, [1] = 'first' })\n"
        "local value = sb:to_table()\n"
        "assert(value.count == 3 and math.type(value.count) == 'integer')\n"
        "assert(value.precise == 1.25 and math.type(value.precise) == 'float')\n"
        "assert(value.nested.ok == true)\n"
        "assert(value[1] == 'first')\n",
        "shared_buffer_roundtrip_test");
}

TEST(SharedBufferTest, ReportsUnsupportedAndMalformedSerializedValues) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    RegisterSharedBuffer(state.Get());

    RunScript(state,
        "local ok, err = pcall(function()\n"
        "  return tas.shared_buffer.from_table({ callback = function() end })\n"
        "end)\n"
        "assert(ok == false and tostring(err):find('unsupported value type', 1, true))\n"
        "local truncated = tas.shared_buffer.from_hex('05')\n"
        "local ok2, err2 = pcall(function() return truncated:to_table() end)\n"
        "assert(ok2 == false and tostring(err2):find('truncated', 1, true))\n",
        "shared_buffer_error_test");
}

TEST(SharedBufferTest, BasicBinaryOperationsRemainAvailable) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    RegisterSharedBuffer(state.Get());

    RunScript(state,
        "local sb = tas.shared_buffer.create(16)\n"
        "assert(sb:size() == 16)\n"
        "sb:write_u8(0, 0x12)\n"
        "sb:write_u16(1, 0x3456)\n"
        "sb:write_i32(4, -123)\n"
        "sb:write_f32(8, 1.25)\n"
        "assert(sb:read_u8(0) == 0x12)\n"
        "assert(sb:read_u16(1) == 0x3456)\n"
        "assert(sb:read_i32(4) == -123)\n"
        "assert(math.abs(sb:read_f32(8) - 1.25) < 0.001)\n"
        "sb:fill(0xaa, 12, 2)\n"
        "assert(sb:to_hex(12, 2) == 'aaaa')\n",
        "shared_buffer_binary_test");
}
