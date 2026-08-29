#include <benchmark/benchmark.h>

#include <Freyr/Freyr.hpp>

#include <cstdint>

struct Position : fr::Component
{
    float x;
    float y;
    float z;
};

struct Velocity : fr::Component
{
    float x;
    float y;
    float z;
};

struct T0 : fr::Component
{
};
struct T1 : fr::Component
{
};
struct T2 : fr::Component
{
};
struct T3 : fr::Component
{
};
struct T4 : fr::Component
{
};
struct T5 : fr::Component
{
};
struct T6 : fr::Component
{
};
struct T7 : fr::Component
{
};
struct T8 : fr::Component
{
};

class BenchApp : public skr::IApplication
{
  public:
    explicit BenchApp(const skr::Arc<skr::ServiceProvider>& rootServiceProvider) :
        IApplication(rootServiceProvider)
    {
        registry = rootServiceProvider->GetService<fr::Registry>();
    }

    void Run() override {}

    skr::Arc<fr::Registry> registry;
};

static void AddTagBit(fr::Registry& registry, fr::Entity entity, int bit)
{
    switch (bit)
    {
        case 0:
            registry.AddComponent(entity, T0 {});
            break;
        case 1:
            registry.AddComponent(entity, T1 {});
            break;
        case 2:
            registry.AddComponent(entity, T2 {});
            break;
        case 3:
            registry.AddComponent(entity, T3 {});
            break;
        case 4:
            registry.AddComponent(entity, T4 {});
            break;
        case 5:
            registry.AddComponent(entity, T5 {});
            break;
        case 6:
            registry.AddComponent(entity, T6 {});
            break;
        case 7:
            registry.AddComponent(entity, T7 {});
            break;
        case 8:
            registry.AddComponent(entity, T8 {});
            break;
        default:
            break;
    }
}

static fr::Entity CreateEntityWithMask(fr::Registry& registry, std::uint32_t mask)
{
    fr::Entity entity = 0;
    bool       first  = true;

    for (int bit = 0; bit < 9; ++bit)
    {
        if ((mask & (1u << bit)) == 0)
            continue;

        if (first)
        {
            switch (bit)
            {
                case 0:
                    entity = registry.CreateEntity(T0 {});
                    break;
                case 1:
                    entity = registry.CreateEntity(T1 {});
                    break;
                case 2:
                    entity = registry.CreateEntity(T2 {});
                    break;
                case 3:
                    entity = registry.CreateEntity(T3 {});
                    break;
                case 4:
                    entity = registry.CreateEntity(T4 {});
                    break;
                case 5:
                    entity = registry.CreateEntity(T5 {});
                    break;
                case 6:
                    entity = registry.CreateEntity(T6 {});
                    break;
                case 7:
                    entity = registry.CreateEntity(T7 {});
                    break;
                case 8:
                    entity = registry.CreateEntity(T8 {});
                    break;
                default:
                    break;
            }
            first = false;
        }
        else
        {
            AddTagBit(registry, entity, bit);
        }
    }

    return entity;
}

static void ArchetypeLookup(benchmark::State& state)
{
    const auto archetypeCount = static_cast<std::uint32_t>(state.range(0));

    const auto app =
        skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                freyr
                    .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                        builder.WithMaxEntities(2 * 1024 * 1024).WithArchetypeChunkCapacity(512);
                    })
                    .WithComponent<T0>()
                    .WithComponent<T1>()
                    .WithComponent<T2>()
                    .WithComponent<T3>()
                    .WithComponent<T4>()
                    .WithComponent<T5>()
                    .WithComponent<T6>()
                    .WithComponent<T7>()
                    .WithComponent<T8>();
            })
            .Build<BenchApp>();

    auto& registry = *app->registry;

    for (std::uint32_t mask = 2; mask <= archetypeCount; ++mask)
    {
        CreateEntityWithMask(registry, mask);
    }
    CreateEntityWithMask(registry, 1);
    registry.ExecuteTasks();

    for (auto _ : state)
    {
        auto entity = registry.CreateEntity(T0 {});
        benchmark::DoNotOptimize(entity);
        registry.DestroyEntity(entity);
        registry.ExecuteTasks();
    }

    state.SetItemsProcessed(state.iterations());
}

static void EachAsyncMultiPass(benchmark::State& state)
{
    const auto entityCount   = static_cast<std::size_t>(state.range(0));
    const auto mutationCount = static_cast<std::size_t>(state.range(1));

    const auto app =
        skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>([entityCount](fr::FreyrExtension& freyr) {
                freyr
                    .WithOptions([entityCount](fr::FreyrOptionsBuilder& builder) {
                        builder.WithMaxEntities(entityCount + 1024)
                            .WithArchetypeChunkCapacity(512)
                            .WithAllPhysicalCores();
                    })
                    .WithComponent<Position>()
                    .WithComponent<Velocity>();
            })
            .Build<BenchApp>();

    auto& registry = *app->registry;

    registry.CreateArchetypeBuilder()
        .WithComponent(Position {})
        .WithComponent(Velocity {})
        .WithEntities(static_cast<fr::Entity>(entityCount))
        .Build();

    for (auto _ : state)
    {
        for (std::size_t i = 0; i < mutationCount; ++i)
        {
            registry.CreateMutation()->EachAsync(
                [](Position& position, Velocity& velocity) {
                    position.x += velocity.x;
                    position.y += velocity.y;
                    position.z += velocity.z;
                });
        }
        registry.ExecuteTasks();
    }

    state.SetItemsProcessed(state.iterations() * entityCount * mutationCount);
}

BENCHMARK(ArchetypeLookup)
    ->Arg(64)
    ->Arg(128)
    ->Arg(256)
    ->Arg(512)
    ->Unit(benchmark::kNanosecond);

BENCHMARK(EachAsyncMultiPass)
    ->Args({ 100'000, 2 })
    ->Args({ 100'000, 4 })
    ->Args({ 100'000, 8 })
    ->Args({ 100'000, 16 })
    ->Args({ 1'000'000, 8 })
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
