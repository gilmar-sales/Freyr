#include <algorithm>
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include <Freyr/Freyr.hpp>

struct SimpleEvent : fr::Event
{
    int value;
};

struct StringEvent : fr::Event
{
    std::string message;
};

struct ComplexEvent : fr::Event
{
    int                 id;
    std::string         data;
    std::vector<double> values;
};

struct CounterEvent
{
    std::atomic<int>* counter;
};

class EventManagerSpec : public ::testing::Test
{
  protected:
    fr::EventManager manager;

    void SetUp() override
    {
        // Reset any global state if needed
    }

    void TearDown() override
    {
        // Cleanup
    }
};

TEST_F(EventManagerSpec, SubscribeAndPublishSingleListener)
{
    int receivedValue = 0;

    auto handler =
        manager.Subscribe<SimpleEvent>([&receivedValue](const SimpleEvent& event) { receivedValue = event.value; });

    manager.Flush();
    auto simpleEvent = SimpleEvent { .value = 42 };
    manager.Send(simpleEvent);

    EXPECT_EQ(receivedValue, 42);
}

TEST_F(EventManagerSpec, SubscribeMultipleListenersToSameEvent)
{
    int              count = 0;
    std::vector<int> receivedValues;

    auto firstHandle = manager.Subscribe<SimpleEvent>([&count](const SimpleEvent& event) { count++; });

    auto secondHandle = manager.Subscribe<SimpleEvent>([&receivedValues](const SimpleEvent& event) {
        receivedValues.push_back(event.value);
    });

    auto thirdHandle = manager.Subscribe<SimpleEvent>([&receivedValues](const SimpleEvent& event) {
        receivedValues.push_back(event.value * 2);
    });

    manager.Flush();
    manager.Send(SimpleEvent { .value = 10 });

    EXPECT_EQ(count, 1);
    EXPECT_EQ(receivedValues[0], 10);
    EXPECT_EQ(receivedValues[1], 20);
}

TEST_F(EventManagerSpec, PublishWithoutSubscribers)
{
    EXPECT_NO_THROW(manager.Send(SimpleEvent { .value = 42 }));
}

TEST_F(EventManagerSpec, SubscribeToMultipleDifferentEvents)
{
    int         simpleValue = 0;
    std::string stringValue;

    auto simpleHandle =
        manager.Subscribe<SimpleEvent>([&simpleValue](const SimpleEvent& event) { simpleValue = event.value; });

    auto stringHandle =
        manager.Subscribe<StringEvent>([&stringValue](const StringEvent& event) { stringValue = event.message; });

    manager.Flush();
    manager.Send(SimpleEvent { .value = 100 });
    manager.Send(StringEvent { .message = "Hello" });

    EXPECT_EQ(simpleValue, 100);
    EXPECT_EQ(stringValue, "Hello");
}

TEST_F(EventManagerSpec, EventsAreIndependent)
{
    int simpleCount = 0;
    int stringCount = 0;

    auto simpleHandle = manager.Subscribe<SimpleEvent>([&simpleCount](const SimpleEvent&) { simpleCount++; });

    auto stringHandle = manager.Subscribe<StringEvent>([&stringCount](const StringEvent&) { stringCount++; });

    manager.Flush();
    manager.Send(SimpleEvent { .value = 1 });
    manager.Send(SimpleEvent { .value = 2 });
    manager.Send(StringEvent { .message = "A" });

    EXPECT_EQ(simpleCount, 2);
    EXPECT_EQ(stringCount, 1);
}

TEST_F(EventManagerSpec, UnsubscribeStopsReceivingEvents)
{
    int count = 0;

    auto handle = manager.Subscribe<SimpleEvent>([&count](const SimpleEvent&) { count++; });

    manager.Flush();

    manager.Send(SimpleEvent { .value = 1 });
    EXPECT_EQ(count, 1);

    handle.reset();
    manager.Send(SimpleEvent { .value = 2 });

    EXPECT_EQ(count, 1);
}

TEST_F(EventManagerSpec, UnsubscribeOneOfMultipleListeners)
{
    int count1 = 0;
    int count2 = 0;

    auto handle1 = manager.Subscribe<SimpleEvent>([&count1](const SimpleEvent&) { count1++; });

    auto handle2 = manager.Subscribe<SimpleEvent>([&count2](const SimpleEvent&) { count2++; });

    manager.Flush();
    manager.Send(SimpleEvent { .value = 1 });
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);

    handle1.reset();
    manager.Send(SimpleEvent { .value = 2 });

    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 2);
}

TEST_F(EventManagerSpec, ConcurrentSubscriptions)
{
    constexpr int kThreadCount            = 10;
    constexpr int kSubscriptionsPerThread = 100;

    std::atomic<int>                     totalCalls { 0 };
    std::vector<std::thread>             threads;
    std::mutex                           mutex;
    std::vector<skr::Arc<fr::ListenerHandle>> handles;

    for (int t = 0; t < kThreadCount; ++t)
    {
        threads.emplace_back([&]() {
            for (int i = 0; i < kSubscriptionsPerThread; ++i)
            {
                std::lock_guard lock(mutex);
                handles.push_back(manager.Subscribe<SimpleEvent>([&totalCalls](const SimpleEvent&) {
                    totalCalls.fetch_add(1, std::memory_order_relaxed);
                }));
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }
    manager.Flush();

    manager.Send(SimpleEvent { .value = 1 });

    EXPECT_EQ(totalCalls.load(), kThreadCount * kSubscriptionsPerThread);
}

TEST_F(EventManagerSpec, ConcurrentPublishing)
{
    constexpr int kThreadCount        = 10;
    constexpr int kPublishesPerThread = 100;

    std::atomic<int> count { 0 };

    auto simpleHandle =
        manager.Subscribe<SimpleEvent>([&count](const SimpleEvent&) { count.fetch_add(1, std::memory_order_relaxed); });

    manager.Flush();
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreadCount; ++t)
    {
        threads.emplace_back([&]() {
            for (int i = 0; i < kPublishesPerThread; ++i)
            {
                manager.Send(SimpleEvent { .value = i });
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(count.load(), kThreadCount * kPublishesPerThread);
}

TEST_F(EventManagerSpec, ConcurrentFlushAndPublish)
{
    constexpr int kDurationMs = 50;

    std::atomic<int>  receiveCount { 0 };
    std::atomic<bool> running { true };

    auto handle = manager.Subscribe<SimpleEvent>(
        [&receiveCount](const SimpleEvent&) { receiveCount.fetch_add(1, std::memory_order_relaxed); });
    manager.Flush();

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back([&]() {
            while (running.load(std::memory_order_relaxed))
            {
                manager.Send(SimpleEvent { .value = 1 });
                std::this_thread::yield();
            }
        });
    }

    threads.emplace_back([&]() {
        while (running.load(std::memory_order_relaxed))
        {
            manager.Flush();
            std::this_thread::yield();
        }
    });

    threads.emplace_back([&]() {
        while (running.load(std::memory_order_relaxed))
        {
            auto h = manager.Subscribe<SimpleEvent>(
                [&receiveCount](const SimpleEvent&) { receiveCount.fetch_add(1, std::memory_order_relaxed); });
            (void)h;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(kDurationMs));
    running.store(false, std::memory_order_relaxed);

    for (auto& thread : threads)
        thread.join();

    manager.Flush();
    manager.Send(SimpleEvent { .value = 2 });

    EXPECT_GT(receiveCount.load(), 0);
    (void)handle;
}

TEST_F(EventManagerSpec, ConcurrentUnsubscribe)
{
    constexpr int                        kThreadCount = 10;
    std::vector<skr::Arc<fr::ListenerHandle>> handles;
    std::atomic<int>                     count { 0 };

    for (int i = 0; i < 100; ++i)
    {
        handles.push_back(manager.Subscribe<SimpleEvent>([&count](const SimpleEvent&) {
            count.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreadCount; ++t)
    {
        threads.emplace_back([&, t]() {
            for (size_t i = t; i < handles.size(); i += kThreadCount)
            {
                handles[i].reset();
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    manager.Send(SimpleEvent { .value = 1 });
    EXPECT_EQ(count.load(), 0);
}

TEST_F(EventManagerSpec, ManyListenersPerformance)
{
    constexpr int                        kListenerCount = 1000;
    std::atomic<int>                     count { 0 };
    std::vector<skr::Arc<fr::ListenerHandle>> handles;

    for (int i = 0; i < kListenerCount; ++i)
    {
        handles.push_back(manager.Subscribe<SimpleEvent>([&count](const SimpleEvent&) {
            count.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    manager.Flush();
    auto start = std::chrono::high_resolution_clock::now();
    manager.Send(SimpleEvent { .value = 1 });
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_EQ(count.load(), kListenerCount);
    EXPECT_LT(duration.count(), 10000);
}

TEST_F(EventManagerSpec, ManyEventsPerformance)
{
    constexpr int    kEventCount = 10000;
    std::atomic<int> count { 0 };

    auto simpleHandle =
        manager.Subscribe<SimpleEvent>([&count](const SimpleEvent&) { count.fetch_add(1, std::memory_order_relaxed); });

    manager.Flush();
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kEventCount; ++i)
    {
        manager.Send(SimpleEvent { .value = i });
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(count.load(), kEventCount);
    EXPECT_LT(duration.count(), 1000);
}

TEST_F(EventManagerSpec, FlushRemovesInactiveListeners)
{
    std::vector<skr::Arc<fr::ListenerHandle>> handles;
    std::atomic<int>                     count { 0 };

    for (int i = 0; i < 100; ++i)
    {
        handles.push_back(manager.Subscribe<SimpleEvent>([&count](const SimpleEvent&) {
            count.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    handles.resize(50);
    manager.Flush();

    manager.Send(SimpleEvent { .value = 1 });

    EXPECT_EQ(count.load(), 50);
}

TEST_F(EventManagerSpec, NoMemoryLeakWithManySubscriptionsAndUnsubscriptions)
{
    for (int iteration = 0; iteration < 100; ++iteration)
    {
        std::vector<skr::Arc<fr::ListenerHandle>> handles;

        for (int i = 0; i < 50; ++i)
        {
            handles.push_back(manager.Subscribe<SimpleEvent>([](const SimpleEvent&) {}));
        }

        handles.clear();

        manager.Flush();
    }

    SUCCEED();
}

TEST_F(EventManagerSpec, ListenerThrowsException)
{
    int count = 0;

    auto handle1 =
        manager.Subscribe<SimpleEvent>([](const SimpleEvent&) { throw std::runtime_error("Test exception"); });

    auto handle2 = manager.Subscribe<SimpleEvent>([&count](const SimpleEvent&) { count++; });

    manager.Flush();
    EXPECT_THROW(manager.Send(SimpleEvent { .value = 1 }), std::runtime_error);
}

TEST_F(EventManagerSpec, ComplexEventWithLargePayload)
{
    std::vector<double> largeVector(10000, 3.14);
    ComplexEvent        received { .id = 0, .data = "", .values = {} };

    auto handle = manager.Subscribe<ComplexEvent>([&received](const ComplexEvent& event) { received = event; });

    manager.Flush();
    ComplexEvent event { .id = 42, .data = "Large payload", .values = largeVector };
    manager.Send(event);

    EXPECT_EQ(received.id, 42);
    EXPECT_EQ(received.data, "Large payload");
    EXPECT_EQ(received.values.size(), 10000);
}

TEST_F(EventManagerSpec, RValueEventOptimization)
{
    std::string receivedMessage;

    auto handle = manager.Subscribe<StringEvent>([&receivedMessage](const StringEvent& event) {
        receivedMessage = event.message;
    });

    manager.Flush();
    std::string largeString(10000, 'X');
    StringEvent event { .message = std::move(largeString) };

    manager.Send(std::move(event));

    EXPECT_EQ(receivedMessage.size(), 10000);
}

TEST_F(EventManagerSpec, SelfUnsubscribeInCallback)
{
    skr::Arc<fr::ListenerHandle> handle;
    int                     count = 0;

    handle = manager.Subscribe<SimpleEvent>([&](const SimpleEvent&) {
        count++;
        handle.reset();
    });
    manager.Flush();

    manager.Send(SimpleEvent { .value = 1 });
    manager.Send(SimpleEvent { .value = 2 });

    EXPECT_EQ(count, 1);
}

TEST_F(EventManagerSpec, SubscribeInCallback)
{
    std::atomic<int>        count { 0 };
    skr::Arc<fr::ListenerHandle> callBackHandle;

    auto handle = manager.Subscribe<SimpleEvent>([&](const SimpleEvent&) {
        count++;
        callBackHandle = manager.Subscribe<SimpleEvent>([&](const SimpleEvent&) { count++; });
    });

    manager.Flush();
    manager.Send(SimpleEvent { .value = 1 });
    EXPECT_EQ(count.load(), 1);

    manager.Send(SimpleEvent { .value = 2 });
    EXPECT_GE(count.load(), 2);
}

TEST_F(EventManagerSpec, StressTestManyEventsAndListeners)
{
    constexpr int                        kEventTypeCount    = 50;
    constexpr int                        kListenersPerEvent = 20;
    constexpr int                        kPublishCount      = 100;
    std::vector<skr::Arc<fr::ListenerHandle>> handles;

    std::atomic<int> totalCount { 0 };

    for (int eventType = 0; eventType < kEventTypeCount; ++eventType)
    {
        for (int listener = 0; listener < kListenersPerEvent; ++listener)
        {
            handles.push_back(manager.Subscribe<SimpleEvent>([&totalCount](const SimpleEvent&) {
                totalCount.fetch_add(1, std::memory_order_relaxed);
            }));
        }
    }

    manager.Flush();
    for (int i = 0; i < kPublishCount; ++i)
    {
        manager.Send(SimpleEvent { .value = i });
    }

    EXPECT_EQ(totalCount.load(), kEventTypeCount * kListenersPerEvent * kPublishCount);
}

// TEST_F(EventManagerSpec, StressTestConcurrentAllOperations)
// {
//     constexpr int     kDuration = 500; // milliseconds
//     std::atomic<bool> running { true };
//     std::atomic<int>  subscribeCount { 0 };
//     std::atomic<int>  publishCount { 0 };
//     std::atomic<int>  unsubscribeCount { 0 };
//     std::atomic<int>  eventCount { 0 };

//     auto worker = [&]() {
//         std::vector<skr::Arc<fr::ListenerHandle>> handles;

//         while (running.load(std::memory_order_acquire))
//         {
//             if (handles.size() < 20)
//             {
//                 auto handle = manager.Subscribe<SimpleEvent>([&eventCount](const SimpleEvent&) {
//                     eventCount.fetch_add(1, std::memory_order_relaxed);
//                 });
//                 handles.push_back(handle);
//                 subscribeCount.fetch_add(1, std::memory_order_relaxed);
//             }

//             manager.Send(SimpleEvent { .value = 1 });
//             publishCount.fetch_add(1, std::memory_order_relaxed);

//             if (!handles.empty() && (rand() % 10) < 3)
//             {
//                 handles.back().reset();
//                 handles.pop_back();
//                 unsubscribeCount.fetch_add(1, std::memory_order_relaxed);
//             }

//             std::this_thread::yield();
//         }
//     };

//     std::vector<std::thread> threads;
//     for (int i = 0; i < 8; ++i)
//     {
//         threads.emplace_back(worker);
//     }

//     std::this_thread::sleep_for(std::chrono::milliseconds(kDuration));
//     running.store(false, std::memory_order_release);

//     for (auto& thread : threads)
//     {
//         thread.join();
//     }

//     EXPECT_GT(subscribeCount.load(), 0);
//     EXPECT_GT(publishCount.load(), 0);
//     EXPECT_GT(eventCount.load(), 0);
// }
