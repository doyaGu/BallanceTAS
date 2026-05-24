#include <gtest/gtest.h>

#include "MessageBus.h"
#include "LuaRuntime/LuaFunction.h"
#include "LuaRuntime/LuaProtectedCall.h"
#include "LuaRuntime/LuaState.h"
#include "LuaRuntime/LuaValue.h"

TEST(MessageBusTest, SendsLuaValuePayloadToCppHandler) {
    MessageBus bus(reinterpret_cast<TASEngine *>(0x1));
    ASSERT_TRUE(bus.Initialize());

    bool called = false;
    bus.RegisterHandler("target", "payload", [&](const MessageBus::Message &message) {
        called = true;
        EXPECT_EQ(message.senderContext, "source");
        EXPECT_EQ(message.data.value.GetIntegerField("answer", 0), 42);
    });

    auto table = std::make_shared<tas::lua::LuaValue::Table>();
    table->entries.push_back({
        tas::lua::LuaValue::Key{std::string("answer")},
        std::make_shared<tas::lua::LuaValue>(static_cast<lua_Integer>(42))
    });

    ASSERT_TRUE(bus.SendMessage("source", "target", "payload", tas::lua::LuaValue{table}));
    bus.ProcessMessages();
    EXPECT_TRUE(called);
}

TEST(MessageBusTest, LuaHandlerReceivesPayloadValue) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    lua_State *L = state.Get();

    MessageBus bus(reinterpret_cast<TASEngine *>(0x1));
    ASSERT_TRUE(bus.Initialize());

    auto load = state.LoadString(
        "received = nil\n"
        "return function(payload)\n"
        "  received = payload\n"
        "end\n",
        "message_bus_lua_handler_test");
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    ASSERT_TRUE(tas::lua::ProtectedCall(L, 0, 1).IsOk());

    bus.RegisterLuaHandler("target", std::weak_ptr<ScriptContext>(), "payload",
                           tas::lua::LuaFunction::FromStack(L, -1));
    ASSERT_TRUE(bus.SendMessage("source", "target", "payload", tas::lua::LuaValue{std::string("hello")}));
    bus.ProcessMessages();

    lua_getglobal(L, "received");
    EXPECT_STREQ(lua_tostring(L, -1), "hello");
    lua_pop(L, 1);

    bus.Shutdown();
}

TEST(MessageBusTest, BroadcastSkipsSenderAndUnsubscribeRemovesHandlers) {
    MessageBus bus(reinterpret_cast<TASEngine *>(0x1));
    ASSERT_TRUE(bus.Initialize());

    int senderCalls = 0;
    int targetCalls = 0;
    bus.RegisterHandler("source", "notice", [&](const MessageBus::Message &) {
        ++senderCalls;
    });
    bus.RegisterHandler("target", "notice", [&](const MessageBus::Message &message) {
        ++targetCalls;
        EXPECT_EQ(message.senderContext, "source");
        EXPECT_EQ(message.targetContext, "*");
    });

    ASSERT_TRUE(bus.BroadcastMessage("source", "notice", tas::lua::LuaValue{std::string("payload")}));
    bus.ProcessMessages();
    EXPECT_EQ(senderCalls, 0);
    EXPECT_EQ(targetCalls, 1);

    bus.RemoveHandler("target", "notice");
    ASSERT_TRUE(bus.BroadcastMessage("source", "notice", tas::lua::LuaValue{std::string("payload")}));
    bus.ProcessMessages();
    EXPECT_EQ(targetCalls, 1);
}

TEST(MessageBusTest, AsyncRequestAndResponseUseCorrelationPayloads) {
    MessageBus bus(reinterpret_cast<TASEngine *>(0x1));
    ASSERT_TRUE(bus.Initialize());

    bus.RegisterHandler("service", "query", [&](const MessageBus::Message &message) {
        EXPECT_FALSE(message.correlationId.empty());
        ASSERT_TRUE(bus.SendResponse("service", message.senderContext, message.correlationId,
                                     tas::lua::LuaValue{std::string("ok")}));
    });

    ASSERT_TRUE(bus.SendRequestAsync("client", "service", "query", tas::lua::LuaValue{static_cast<lua_Integer>(7)},
                                    "corr-1"));
    bus.ProcessMessages();
    bus.ProcessMessages();

    auto response = bus.TryGetResponse("corr-1");
    ASSERT_TRUE(response.has_value());
    EXPECT_TRUE(response->isResponse);
    EXPECT_EQ(response->senderContext, "service");
    EXPECT_EQ(response->targetContext, "client");

    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    response->data.value.Push(state.Get());
    EXPECT_STREQ(lua_tostring(state.Get(), -1), "ok");
    lua_pop(state.Get(), 1);
}
