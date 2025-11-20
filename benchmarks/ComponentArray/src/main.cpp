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
    auto taskManager = skr::MakeRef<fr::TaskManager>(
        options, skr::MakeRef<skr::Logger<fr::TaskManager>>(skr::MakeRef<skr::LoggerOptions>()));
    std::string internalName   = "empty";
    auto        archetypeChunk = fr::ArchetypeChunk(&internalName, {}, options, taskManager);
    archetypeChunk.AddComponentArray<Position>();

    for (int i = 0; i < state.range(0); ++i)
    {
        archetypeChunk.TryAddEntity(i);
    }

    auto positionArray = archetypeChunk.GetComponentArray<Position>();
    for (auto _ : state)
    {
        for (int i = 0; i < state.range(0); ++i)
        {
            positionArray->GetComponent(archetypeChunk.getIndex(i)).x = i;
        }
    }
}

BENCHMARK(ArchetypeChunkIteration)->RangeMultiplier(2)->Range(100'000, 1'000'000);
BENCHMARK(ComponentArrayIteration)->RangeMultiplier(2)->Range(100'000, 1'000'000);

BENCHMARK_MAIN();
