#include <gtest/gtest.h>

#include "EventBus.h"

// ── Test event types ────────────────────────────────────────────────────────
struct TestEvent {
    int value = 0;
};

struct AnotherEvent {
    std::string msg;
};

// ── Basic publish / subscribe ───────────────────────────────────────────────

TEST(EventBusTest, SubscribeReceivesPublishedEvent) {
    EventBus bus;
    int received = 0;

    auto sub = bus.Subscribe<TestEvent>([&](const TestEvent &e) {
        received = e.value;
    });

    bus.Publish(TestEvent{42});
    EXPECT_EQ(received, 42);
}

TEST(EventBusTest, MultipleSubscribersAllReceive) {
    EventBus bus;
    int a = 0, b = 0;

    auto sub1 = bus.Subscribe<TestEvent>([&](const TestEvent &e) { a = e.value; });
    auto sub2 = bus.Subscribe<TestEvent>([&](const TestEvent &e) { b = e.value; });

    bus.Publish(TestEvent{7});
    EXPECT_EQ(a, 7);
    EXPECT_EQ(b, 7);
}

TEST(EventBusTest, DifferentEventTypesAreIndependent) {
    EventBus bus;
    int intVal = 0;
    std::string strVal;

    auto sub1 = bus.Subscribe<TestEvent>([&](const TestEvent &e) { intVal = e.value; });
    auto sub2 = bus.Subscribe<AnotherEvent>([&](const AnotherEvent &e) { strVal = e.msg; });

    bus.Publish(TestEvent{99});
    EXPECT_EQ(intVal, 99);
    EXPECT_TRUE(strVal.empty());

    bus.Publish(AnotherEvent{"hello"});
    EXPECT_EQ(strVal, "hello");
    EXPECT_EQ(intVal, 99); // unchanged
}

// ── RAII unsubscription ─────────────────────────────────────────────────────

TEST(EventBusTest, ScopedSubscriptionUnsubscribesOnDestruction) {
    EventBus bus;
    int callCount = 0;

    {
        auto sub = bus.Subscribe<TestEvent>([&](const TestEvent &) { ++callCount; });
        bus.Publish(TestEvent{});
        EXPECT_EQ(callCount, 1);
    } // sub destroyed here

    bus.Publish(TestEvent{});
    EXPECT_EQ(callCount, 1); // not called again
}

TEST(EventBusTest, ScopedSubscriptionMoveTransfersOwnership) {
    EventBus bus;
    int callCount = 0;

    ScopedSubscription outer;
    {
        auto inner = bus.Subscribe<TestEvent>([&](const TestEvent &) { ++callCount; });
        outer = std::move(inner);
    }
    // inner destroyed, but ownership moved to outer

    bus.Publish(TestEvent{});
    EXPECT_EQ(callCount, 1);

    outer = ScopedSubscription{}; // explicitly release
    bus.Publish(TestEvent{});
    EXPECT_EQ(callCount, 1); // not called again
}

// ── Edge cases ──────────────────────────────────────────────────────────────

TEST(EventBusTest, PublishWithNoSubscribersDoesNotCrash) {
    EventBus bus;
    EXPECT_NO_THROW(bus.Publish(TestEvent{1}));
}

TEST(EventBusTest, SubscribeDuringPublishIsSafe) {
    EventBus bus;
    int secondCalls = 0;
    ScopedSubscription lateSub;
    bool alreadySubscribed = false;

    auto sub = bus.Subscribe<TestEvent>([&](const TestEvent &) {
        if (!alreadySubscribed) {
            alreadySubscribed = true;
            lateSub = bus.Subscribe<TestEvent>([&](const TestEvent &) { ++secondCalls; });
        }
    });

    // First publish: the first handler subscribes the second handler.
    bus.Publish(TestEvent{});

    // Second publish: both old and new handler should fire. The second handler
    // definitely exists now, so it should be called at least once.
    bus.Publish(TestEvent{});
    EXPECT_GE(secondCalls, 1);
}

TEST(EventBusTest, UnsubscribeDuringPublishIsSafe) {
    EventBus bus;
    int calls = 0;
    ScopedSubscription self;

    self = bus.Subscribe<TestEvent>([&](const TestEvent &) {
        ++calls;
        self = ScopedSubscription{}; // unsubscribe self during dispatch
    });

    bus.Publish(TestEvent{});
    EXPECT_EQ(calls, 1);

    bus.Publish(TestEvent{});
    EXPECT_EQ(calls, 1); // not called again
}
