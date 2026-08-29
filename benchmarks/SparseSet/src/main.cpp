#include <benchmark/benchmark.h>

#include <Freyr/Freyr.hpp>

#include <vector>

struct Position : fr::Component
{
    float x;
    float y;
    float z;
};

static void SparseSetGetIndexLocked(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    auto       set   = fr::SparseSet<fr::Entity>(static_cast<unsigned>(count));

    for (std::size_t i = 0; i < count; ++i)
    {
        set.insert(static_cast<fr::Entity>(i));
    }

    std::size_t cursor = 0;
    for (auto _ : state)
    {
        auto index = set.getIndex(static_cast<fr::Entity>(cursor));
        benchmark::DoNotOptimize(index);
        cursor = (cursor + 1) % count;
    }

    state.SetItemsProcessed(state.iterations());
}

static void SparseSetGetIndexLocal(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    auto       set   = fr::LocalSparseSet<fr::Entity>(static_cast<unsigned>(count));

    for (std::size_t i = 0; i < count; ++i)
    {
        set.insert(static_cast<fr::Entity>(i));
    }

    std::size_t cursor = 0;
    for (auto _ : state)
    {
        auto index = set.getIndex(static_cast<fr::Entity>(cursor));
        benchmark::DoNotOptimize(index);
        cursor = (cursor + 1) % count;
    }

    state.SetItemsProcessed(state.iterations());
}

static void SparseSetContainsLocked(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    auto       set   = fr::SparseSet<fr::Entity>(static_cast<unsigned>(count));

    for (std::size_t i = 0; i < count; ++i)
    {
        set.insert(static_cast<fr::Entity>(i));
    }

    std::size_t cursor = 0;
    for (auto _ : state)
    {
        bool found = set.contains(static_cast<fr::Entity>(cursor));
        benchmark::DoNotOptimize(found);
        cursor = (cursor + 1) % count;
    }

    state.SetItemsProcessed(state.iterations());
}

static void SparseSetContainsLocal(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    auto       set   = fr::LocalSparseSet<fr::Entity>(static_cast<unsigned>(count));

    for (std::size_t i = 0; i < count; ++i)
    {
        set.insert(static_cast<fr::Entity>(i));
    }

    std::size_t cursor = 0;
    for (auto _ : state)
    {
        bool found = set.contains(static_cast<fr::Entity>(cursor));
        benchmark::DoNotOptimize(found);
        cursor = (cursor + 1) % count;
    }

    state.SetItemsProcessed(state.iterations());
}

static void SparseSetInsertRemoveLocked(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    auto       set   = fr::SparseSet<fr::Entity>(static_cast<unsigned>(count * 2));

    for (std::size_t i = 0; i < count; ++i)
    {
        set.insert(static_cast<fr::Entity>(i));
    }

    fr::Entity next = static_cast<fr::Entity>(count);
    for (auto _ : state)
    {
        set.insert(next);
        set.remove(next);
        benchmark::DoNotOptimize(set.size());
        ++next;
    }

    state.SetItemsProcessed(state.iterations());
}

static void SparseSetInsertRemoveLocal(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    auto       set   = fr::LocalSparseSet<fr::Entity>(static_cast<unsigned>(count * 2));

    for (std::size_t i = 0; i < count; ++i)
    {
        set.insert(static_cast<fr::Entity>(i));
    }

    fr::Entity next = static_cast<fr::Entity>(count);
    for (auto _ : state)
    {
        set.insert(next);
        set.remove(next);
        benchmark::DoNotOptimize(set.size());
        ++next;
    }

    state.SetItemsProcessed(state.iterations());
}

static void ArchetypeChunkGetComponent(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));

    auto options     = fr::FreyrOptionsBuilder().WithArchetypeChunkCapacity(count).Build();
    auto taskCounter = skr::MakeArc<fr::TaskCounter>();
    auto threadPool  = skr::MakeArc<fr::ThreadPool>(
        options,
        skr::MakeArc<skr::Logger<fr::ThreadPool>>(skr::MakeArc<skr::LoggerOptions>()),
        taskCounter);

    std::string internalName = "SparseSetBench";
    auto        chunk        = fr::ArchetypeChunk(internalName, options, threadPool, taskCounter);
    chunk.AddComponentArray<Position>();

    std::vector<fr::Entity> entities(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        entities[i] = static_cast<fr::Entity>(i);
        chunk.TryAddEntity(entities[i]);
        chunk.AddComponent(entities[i], Position { .x = static_cast<float>(i) });
    }

    std::size_t cursor = 0;
    for (auto _ : state)
    {
        auto& position = chunk.GetComponent<Position>(entities[cursor]);
        benchmark::DoNotOptimize(position.x);
        cursor = (cursor + 1) % count;
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(SparseSetGetIndexLocked)->Arg(512)->Arg(4096)->Arg(65536)->Unit(benchmark::kNanosecond);
BENCHMARK(SparseSetGetIndexLocal)->Arg(512)->Arg(4096)->Arg(65536)->Unit(benchmark::kNanosecond);
BENCHMARK(SparseSetContainsLocked)->Arg(512)->Arg(4096)->Arg(65536)->Unit(benchmark::kNanosecond);
BENCHMARK(SparseSetContainsLocal)->Arg(512)->Arg(4096)->Arg(65536)->Unit(benchmark::kNanosecond);
BENCHMARK(SparseSetInsertRemoveLocked)->Arg(512)->Arg(4096)->Unit(benchmark::kNanosecond);
BENCHMARK(SparseSetInsertRemoveLocal)->Arg(512)->Arg(4096)->Unit(benchmark::kNanosecond);
BENCHMARK(ArchetypeChunkGetComponent)->Arg(512)->Arg(4096)->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
