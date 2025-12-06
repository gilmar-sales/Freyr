#include <Freyr/Freyr.hpp>
#include <benchmark/benchmark.h>

struct Position : fr::Component
{
    float x;
    float y;
    float z;
};

static void ComponentArrayIteration(benchmark::State& state)
{
    auto options       = fr::FreyrOptionsBuilder().SetArchetypeChunkCapacity(state.range(0)).Build();
    auto positionArray = fr::ComponentArray<Position>(options);

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
    auto options     = fr::FreyrOptionsBuilder().SetArchetypeChunkCapacity(state.range(0)).Build();
    auto taskCounter = skr::MakeRef<fr::TaskCounter>();
    auto taskManager = skr::MakeRef<fr::TaskManager>(
        options, skr::MakeRef<skr::Logger<fr::TaskManager>>(skr::MakeRef<skr::LoggerOptions>()), taskCounter);
    std::string internalName   = "empty";
    auto        archetypeChunk = fr::ArchetypeChunk(&internalName, {}, options, taskManager, taskCounter);
    archetypeChunk.AddComponentArray<Position>();

    for (int i = 0; i < state.range(0); ++i)
    {
        archetypeChunk.TryAddEntity(i);
    }

    for (auto _ : state)
    {
        archetypeChunk.ForEach<Position>("bench", [](auto entity, Position& position) { position.x = entity; });
    }
}

BENCHMARK(ArchetypeChunkIteration)->RangeMultiplier(2)->Range(100'000, 1'000'000);
BENCHMARK(ComponentArrayIteration)->RangeMultiplier(2)->Range(100'000, 1'000'000);

BENCHMARK_MAIN();
