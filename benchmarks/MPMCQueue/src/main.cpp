#include <benchmark/benchmark.h>

#include <Freyr/Containers/MPMCQueue.hpp>
#include <Freyr/Containers/UnboundedMPMCQueue.hpp>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace
{
    constexpr std::uint32_t Operations = 1000;

    template <typename Queue>
    Queue MakeQueue()
    {
        if constexpr (requires { Queue(1024); })
            return Queue(1024);
        else
            return Queue();
    }

    template <typename Queue>
    void Spsc(benchmark::State& state)
    {
        for (auto _ : state)
        {
            auto queue = MakeQueue<Queue>();
            std::thread producer([&queue] {
                for (std::uint32_t i = 0; i < Operations; ++i)
                    queue.push(i);
            });
            std::thread consumer([&queue] {
                for (std::uint32_t i = 0; i < Operations; ++i)
                {
                    std::uint32_t value;
                    queue.pop(value);
                    benchmark::DoNotOptimize(value);
                }
            });
            producer.join();
            consumer.join();
        }
        state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * Operations * 2);
    }

    template <typename Queue>
    void Mpmc(benchmark::State& state)
    {
        const auto workerCount = static_cast<std::uint32_t>(state.range(0));
        const auto totalWork   = workerCount * Operations / 2;
        for (auto _ : state)
        {
            auto             queue = MakeQueue<Queue>();
            std::atomic_uint32_t consumed = 0;
            std::vector<std::thread> workers;
            workers.reserve(workerCount * 2);

            for (std::uint32_t worker = 0; worker < workerCount; ++worker)
            {
                workers.emplace_back([&queue] {
                    for (std::uint32_t i = 0; i < Operations / 2; ++i)
                        queue.push(i);
                });
                workers.emplace_back([&queue, &consumed, totalWork] {
                    while (consumed.load(std::memory_order_relaxed) < totalWork)
                    {
                        std::uint32_t value;
                        if (queue.try_pop(value))
                        {
                            benchmark::DoNotOptimize(value);
                            consumed.fetch_add(1, std::memory_order_relaxed);
                        }
                        else
                            std::this_thread::yield();
                    }
                });
            }

            for (auto& worker : workers)
                worker.join();
        }
        state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * workerCount * Operations);
    }
}

using BoundedQueue   = rigtorp::MPMCQueue<std::uint32_t>;
using UnboundedQueue = rigtorp::UnboundedMPMCQueue<std::uint32_t>;

BENCHMARK_TEMPLATE(Spsc, BoundedQueue)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(Spsc, UnboundedQueue)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(Mpmc, BoundedQueue)->Arg(1)->Arg(4)->Arg(8)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(Mpmc, UnboundedQueue)->Arg(1)->Arg(4)->Arg(8)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
