#include <benchmark/benchmark.h>

#include <Freyr/Freyr.hpp>

#include <array>
#include <memory>

struct C0 : fr::Component
{
    float v;
};
struct C1 : fr::Component
{
    float v;
};
struct C2 : fr::Component
{
    float v;
};
struct C3 : fr::Component
{
    float v;
};
struct C4 : fr::Component
{
    float v;
};
struct C5 : fr::Component
{
    float v;
};
struct C6 : fr::Component
{
    float v;
};
struct C7 : fr::Component
{
    float v;
};

using AddArrayFn = void (*)(fr::ArchetypeChunk&);

template <typename T>
void AddArray(fr::ArchetypeChunk& chunk)
{
    chunk.AddComponentArray<T>();
}

static constexpr std::array<AddArrayFn, 8> kAddArrayFns = {
    &AddArray<C0>, &AddArray<C1>, &AddArray<C2>, &AddArray<C3>,
    &AddArray<C4>, &AddArray<C5>, &AddArray<C6>, &AddArray<C7>,
};

static void FillComponents(
    fr::ArchetypeChunk& chunk, fr::Entity entity, int componentCount, float value)
{
    chunk.AddComponent(entity, C0 { .v = value });
    if (componentCount > 1)
        chunk.AddComponent(entity, C1 { .v = value });
    if (componentCount > 2)
        chunk.AddComponent(entity, C2 { .v = value });
    if (componentCount > 3)
        chunk.AddComponent(entity, C3 { .v = value });
    if (componentCount > 4)
        chunk.AddComponent(entity, C4 { .v = value });
    if (componentCount > 5)
        chunk.AddComponent(entity, C5 { .v = value });
    if (componentCount > 6)
        chunk.AddComponent(entity, C6 { .v = value });
    if (componentCount > 7)
        chunk.AddComponent(entity, C7 { .v = value });
}

static std::unique_ptr<fr::ArchetypeChunk> MakeChunk(
    std::size_t                 capacity,
    int                         componentCount,
    const char*                 name,
    skr::Arc<fr::FreyrOptions>& options,
    skr::Arc<fr::TaskCounter>&  taskCounter,
    skr::Arc<fr::ThreadPool>&   threadPool)
{
    auto chunk = std::make_unique<fr::ArchetypeChunk>(name, options, threadPool, taskCounter);
    for (int i = 0; i < componentCount; ++i)
    {
        kAddArrayFns[static_cast<std::size_t>(i)](*chunk);
    }
    return chunk;
}

static void ChunkRemoveEntity(benchmark::State& state)
{
    const auto capacity       = static_cast<std::size_t>(state.range(0));
    const auto componentCount = static_cast<int>(state.range(1));

    auto options     = fr::FreyrOptionsBuilder().WithArchetypeChunkCapacity(capacity).Build();
    auto taskCounter = skr::MakeArc<fr::TaskCounter>();
    auto threadPool  = skr::MakeArc<fr::ThreadPool>(
        options,
        skr::MakeArc<skr::Logger<fr::ThreadPool>>(skr::MakeArc<skr::LoggerOptions>()),
        taskCounter);

    auto chunk = MakeChunk(capacity, componentCount, "Remove", options, taskCounter, threadPool);

    for (std::size_t i = 0; i < capacity; ++i)
    {
        const auto entity = static_cast<fr::Entity>(i);
        chunk->TryAddEntity(entity);
        FillComponents(*chunk, entity, componentCount, static_cast<float>(i));
    }

    fr::Entity nextId = static_cast<fr::Entity>(capacity);
    for (auto _ : state)
    {
        const auto removing = chunk->GetEntityAt(0);
        chunk->RemoveEntity(removing);

        state.PauseTiming();
        chunk->TryAddEntity(nextId);
        FillComponents(*chunk, nextId, componentCount, static_cast<float>(nextId));
        ++nextId;
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations());
}

static void ChunkMoveData(benchmark::State& state)
{
    const auto capacity       = static_cast<std::size_t>(state.range(0));
    const auto componentCount = static_cast<int>(state.range(1));

    auto options     = fr::FreyrOptionsBuilder().WithArchetypeChunkCapacity(capacity).Build();
    auto taskCounter = skr::MakeArc<fr::TaskCounter>();
    auto threadPool  = skr::MakeArc<fr::ThreadPool>(
        options,
        skr::MakeArc<skr::Logger<fr::ThreadPool>>(skr::MakeArc<skr::LoggerOptions>()),
        taskCounter);

    auto source = MakeChunk(capacity, componentCount, "MoveSrc", options, taskCounter, threadPool);
    auto target = MakeChunk(capacity, componentCount, "MoveDst", options, taskCounter, threadPool);

    const auto liveCount = capacity - 1;
    for (std::size_t i = 0; i < liveCount; ++i)
    {
        const auto entity = static_cast<fr::Entity>(i);
        source->TryAddEntity(entity);
        FillComponents(*source, entity, componentCount, static_cast<float>(i));
    }

    for (auto _ : state)
    {
        const auto moving = source->GetEntityAt(0);

        state.PauseTiming();
        target->TryAddEntity(moving);
        state.ResumeTiming();

        source->MoveData(moving, target.get());

        state.PauseTiming();
        target->RemoveEntity(moving);
        source->TryAddEntity(moving);
        FillComponents(*source, moving, componentCount, 1.f);
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations());
}

static void ChunkSwap(benchmark::State& state)
{
    const auto capacity       = static_cast<std::size_t>(state.range(0));
    const auto componentCount = static_cast<int>(state.range(1));

    auto options     = fr::FreyrOptionsBuilder().WithArchetypeChunkCapacity(capacity).Build();
    auto taskCounter = skr::MakeArc<fr::TaskCounter>();
    auto threadPool  = skr::MakeArc<fr::ThreadPool>(
        options,
        skr::MakeArc<skr::Logger<fr::ThreadPool>>(skr::MakeArc<skr::LoggerOptions>()),
        taskCounter);

    auto chunk = MakeChunk(capacity, componentCount, "Swap", options, taskCounter, threadPool);

    for (std::size_t i = 0; i < capacity; ++i)
    {
        const auto entity = static_cast<fr::Entity>(i);
        chunk->TryAddEntity(entity);
        FillComponents(*chunk, entity, componentCount, static_cast<float>(i));
    }

    for (auto _ : state)
    {
        chunk->Swap(0, 1);
        benchmark::DoNotOptimize(chunk->GetComponentAt<C0>(0).v);
        benchmark::DoNotOptimize(chunk->GetComponentAt<C0>(1).v);
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(ChunkRemoveEntity)
    ->Args({ 512, 2 })
    ->Args({ 512, 4 })
    ->Args({ 512, 8 })
    ->Unit(benchmark::kNanosecond);

BENCHMARK(ChunkMoveData)
    ->Args({ 512, 2 })
    ->Args({ 512, 4 })
    ->Args({ 512, 8 })
    ->Unit(benchmark::kNanosecond);

BENCHMARK(ChunkSwap)
    ->Args({ 512, 2 })
    ->Args({ 512, 4 })
    ->Args({ 512, 8 })
    ->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
