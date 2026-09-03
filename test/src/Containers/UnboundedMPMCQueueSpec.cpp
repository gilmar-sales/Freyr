#include <Freyr/Containers/UnboundedMPMCQueue.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

TEST(UnboundedMPMCQueueSpec, ShouldGrowAcrossSegments)
{
    rigtorp::UnboundedMPMCQueue<std::uint32_t, 8> queue;

    for (std::uint32_t value = 0; value < 1000; ++value)
        queue.push(value);

    EXPECT_EQ(queue.size(), 1000);
    for (std::uint32_t value = 0; value < 1000; ++value)
    {
        std::uint32_t result = 0;
        ASSERT_TRUE(queue.try_pop(result));
        EXPECT_EQ(result, value);
    }
    EXPECT_TRUE(queue.empty());
}

TEST(UnboundedMPMCQueueSpec, TryPopShouldReturnFalseWhenEmpty)
{
    rigtorp::UnboundedMPMCQueue<int, 8> queue;
    int                                 value = 0;

    EXPECT_FALSE(queue.try_pop(value));
    EXPECT_TRUE(queue.try_push(42));
    EXPECT_TRUE(queue.try_pop(value));
    EXPECT_EQ(value, 42);
}

TEST(UnboundedMPMCQueueSpec, ShouldDeliverEveryValueWithMultipleProducersAndConsumers)
{
    constexpr std::uint32_t producerCount     = 4;
    constexpr std::uint32_t consumerCount     = 4;
    constexpr std::uint32_t valuesPerProducer = 2000;
    constexpr std::uint32_t valueCount        = producerCount * valuesPerProducer;

    rigtorp::UnboundedMPMCQueue<std::uint32_t, 32> queue;
    std::vector<std::atomic_uint32_t>              seen(valueCount);
    std::atomic_uint32_t                           consumed = 0;
    std::vector<std::thread>                       producers;
    std::vector<std::thread>                       consumers;

    for (auto& count : seen)
        count.store(0, std::memory_order_relaxed);

    for (std::uint32_t producer = 0; producer < producerCount; ++producer)
    {
        producers.emplace_back([&queue, producer] {
            for (std::uint32_t value = 0; value < valuesPerProducer; ++value)
                queue.push(producer * valuesPerProducer + value);
        });
    }

    for (std::uint32_t consumer = 0; consumer < consumerCount; ++consumer)
    {
        consumers.emplace_back([&queue, &seen, &consumed, valueCount] {
            while (consumed.load(std::memory_order_relaxed) < valueCount)
            {
                std::uint32_t value = 0;
                if (queue.try_pop(value))
                {
                    ASSERT_LT(value, valueCount);
                    seen[value].fetch_add(1, std::memory_order_relaxed);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
                else
                    std::this_thread::yield();
            }
        });
    }

    for (auto& producer : producers)
        producer.join();
    for (auto& consumer : consumers)
        consumer.join();

    EXPECT_EQ(consumed.load(), valueCount);
    for (const auto& count : seen)
        EXPECT_EQ(count.load(), 1);
    EXPECT_TRUE(queue.empty());
}

TEST(UnboundedMPMCQueueSpec, ShouldCrossSegmentWhileConsumerIsActive)
{
    constexpr std::uint32_t                       valueCount = 10000;
    rigtorp::UnboundedMPMCQueue<std::uint32_t, 4> queue;
    std::atomic_uint32_t                          consumed = 0;

    std::thread consumer([&] {
        while (consumed.load(std::memory_order_relaxed) < valueCount)
        {
            std::uint32_t value = 0;
            if (queue.try_pop(value))
            {
                EXPECT_EQ(value, consumed.load(std::memory_order_relaxed));
                consumed.fetch_add(1, std::memory_order_relaxed);
            }
            else
                std::this_thread::yield();
        }
    });

    for (std::uint32_t value = 0; value < valueCount; ++value)
        queue.push(value);

    consumer.join();
    EXPECT_EQ(consumed.load(), valueCount);
    EXPECT_TRUE(queue.empty());
}
