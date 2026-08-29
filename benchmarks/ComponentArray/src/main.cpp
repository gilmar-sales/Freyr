#include <benchmark/benchmark.h>

#include <Freyr/Freyr.hpp>

struct Position : fr::Component
{
    float x;
    float y;
    float z;
};

static void ComponentArrayIteration(benchmark::State& state)
{
    auto positionArray = fr::ComponentArray<Position>(state.range(0));

    for (auto _ : state)
    {
        for (int i = 0; i < state.range(0); ++i)
        {
            positionArray[i].x = i;
        }
    }
}

static void ArchetypeChunkIteration(benchmark::State& state)
{
    auto options     = fr::FreyrOptionsBuilder().WithArchetypeChunkCapacity(state.range(0)).Build();
    auto taskCounter = skr::MakeArc<fr::TaskCounter>();
    auto threadPool  = skr::MakeArc<fr::ThreadPool>(
        options, skr::MakeArc<skr::Logger<fr::ThreadPool>>(skr::MakeArc<skr::LoggerOptions>()),
        taskCounter);
    std::string internalName   = "empty";
    auto        archetypeChunk = fr::ArchetypeChunk(internalName, options, threadPool, taskCounter);
    archetypeChunk.AddComponentArray<Position>();

    for (int i = 0; i < state.range(0); ++i)
    {
        archetypeChunk.TryAddEntity(i);
    }

    for (auto _ : state)
    {
        archetypeChunk.ForEach<Position>("chunk", [](auto entity, Position& position) {
            position.x = entity;
        });
    }
}

static void ArchetypeIteration(benchmark::State& state)
{
    auto options     = fr::FreyrOptionsBuilder().Build();
    auto taskCounter = skr::MakeArc<fr::TaskCounter>();
    auto threadPool  = skr::MakeArc<fr::ThreadPool>(
        options, skr::MakeArc<skr::Logger<fr::ThreadPool>>(skr::MakeArc<skr::LoggerOptions>()),
        taskCounter);
    auto archetype = fr::Archetype(options, threadPool, taskCounter);

    archetype.RegisterComponent<Position>();
    for (int i = 0; i < state.range(0); ++i)
    {
        archetype.AddEntity(i);
    }

    for (auto _ : state)
    {
        archetype.ForEach<Position>("archetype", [](auto entity, Position& position) {
            position.x = entity;
        });
    }
}

BENCHMARK(ComponentArrayIteration)->RangeMultiplier(2)->Range(100'000, 1'000'000);
BENCHMARK(ArchetypeChunkIteration)->RangeMultiplier(2)->Range(100'000, 1'000'000);
BENCHMARK(ArchetypeIteration)->RangeMultiplier(2)->Range(100'000, 1'000'000);
BENCHMARK_MAIN();