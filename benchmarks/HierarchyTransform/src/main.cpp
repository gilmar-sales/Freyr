#include <benchmark/benchmark.h>

#include <Freyr/Freyr.hpp>
#include <Freyr/Hierarchy/Policies/Mat4TransformPolicy.hpp>

#include <cstdint>
#include <random>
#include <vector>

namespace
{
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

    auto CreateRegistry(std::size_t maxEntities = 150'000, std::uint64_t threadCount = 4)
    {
        return skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>([maxEntities, threadCount](fr::FreyrExtension& freyr) {
                freyr.WithHierarchyPropagation<fr::Mat4TransformPolicy>().WithOptions(
                    [maxEntities, threadCount](fr::FreyrOptionsBuilder& builder) {
                        builder.WithMaxEntities(maxEntities)
                            .WithArchetypeChunkCapacity(512)
                            .WithThreadCount(threadCount);
                    });
            })
            .Build<BenchApp>();
    }

    enum class Topology
    {
        Wide,
        Deep,
        Large,
        Huge,
        Massive
    };

    struct TreeParams
    {
        std::uint32_t depth       = 4;
        std::uint32_t branching   = 8;
        std::uint32_t targetNodes = 4'000;
    };

    TreeParams ParamsFor(Topology topology)
    {
        switch (topology)
        {
            case Topology::Wide:
                return {.depth = 3, .branching = 16, .targetNodes = 4'000};
            case Topology::Deep:
                return {.depth = 20, .branching = 2, .targetNodes = 2'000};
            case Topology::Large:
                return {.depth = 8, .branching = 4, .targetNodes = 12'000};
            case Topology::Huge:
                return {.depth = 12, .branching = 4, .targetNodes = 50'000};
            case Topology::Massive:
                return {.depth = 14, .branching = 4, .targetNodes = 100'000};
        }
        return {};
    }

    struct TreeBuild
    {
        std::vector<fr::Entity> entities;
        fr::Entity              root = fr::NullEntity;
    };

    TreeBuild SpawnTree(fr::Registry& registry, TreeParams params, std::uint32_t seed = 42)
    {
        std::mt19937 rng(seed);
        TreeBuild    tree;

        tree.root = registry.CreateEntity(fr::LocalTransform3D {}, fr::WorldTransform3D {});
        tree.entities.push_back(tree.root);

        std::vector<fr::Entity> frontier { tree.root };
        std::uint32_t           created = 1;

        while (!frontier.empty() && created < params.targetNodes)
        {
            std::vector<fr::Entity> next;
            for (const fr::Entity parent : frontier)
            {
                if (registry.GetDepth(parent) >= params.depth)
                    continue;

                for (std::uint32_t i = 0; i < params.branching && created < params.targetNodes; ++i)
                {
                    const float t     = static_cast<float>(rng() % 1000) / 1000.f;
                    const auto  child = registry.CreateEntity(fr::TranslationLocal3D(t, 0.f, 0.f),
                                                              fr::WorldTransform3D {});
                    registry.SetParent(child, parent);
                    tree.entities.push_back(child);
                    next.push_back(child);
                    ++created;

                    if ((created % 256u) == 0u)
                        registry.ExecuteTasks();
                }
            }
            frontier = std::move(next);
        }

        registry.ExecuteTasks();
        return tree;
    }

    TreeBuild SpawnTree(fr::Registry& registry, Topology topology, std::uint32_t seed = 42)
    {
        return SpawnTree(registry, ParamsFor(topology), seed);
    }

    float SumWorldDiagonal(fr::Registry& registry)
    {
        float sum = 0.f;
        registry.CreateMutation()->Each([&](fr::WorldTransform3D& world) {
            sum += world.matrix[0] + world.matrix[5] + world.matrix[10] + world.matrix[15];
        });
        return sum;
    }
} // namespace

static void BM_Hierarchy_SetParent(benchmark::State& state)
{
    const auto n        = static_cast<std::size_t>(state.range(0));
    auto       app      = CreateRegistry(n + 16);
    auto&      registry = *app->registry;

    std::vector<fr::Entity> entities;
    entities.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        entities.push_back(registry.CreateEntity());
        if ((i % 256u) == 0u)
            registry.ExecuteTasks();
    }
    registry.ExecuteTasks();

    const auto root = entities.front();
    for (auto _ : state)
    {
        for (std::size_t i = 1; i < entities.size(); ++i)
            registry.SetParent(entities[i], root);
        registry.ExecuteTasks();
        for (std::size_t i = 1; i < entities.size(); ++i)
            registry.ClearParent(entities[i]);
        registry.ExecuteTasks();
        benchmark::DoNotOptimize(registry.Children(root).size());
    }
}

static void BM_Hierarchy_ChildrenIterate(benchmark::State& state)
{
    auto       app      = CreateRegistry();
    auto&      registry = *app->registry;
    const auto tree     = SpawnTree(registry, Topology::Wide);

    for (auto _ : state)
    {
        std::size_t count = 0;
        registry.ForEachChild(tree.root, [&](fr::Entity child) {
            registry.ForEachDescendant(child, [&](fr::Entity) { ++count; });
        });
        benchmark::DoNotOptimize(count);
    }
}

static void BM_Hierarchy_CascadeDestroy(benchmark::State& state)
{
    const auto topology = static_cast<Topology>(state.range(0));

    for (auto _ : state)
    {
        state.PauseTiming();
        auto       app      = CreateRegistry(ParamsFor(topology).targetNodes + 1'000);
        auto&      registry = *app->registry;
        const auto tree     = SpawnTree(registry, topology);
        state.ResumeTiming();

        registry.DestroyEntity(tree.root);
        registry.ExecuteTasks();
        benchmark::DoNotOptimize(registry.Children(tree.root).size());
    }
}

static void BM_Propagate_Static(benchmark::State& state)
{
    const auto topology = static_cast<Topology>(state.range(0));
    const auto params   = ParamsFor(topology);
    auto       app      = CreateRegistry(params.targetNodes + 1'000);
    auto&      registry = *app->registry;
    SpawnTree(registry, params);

    for (auto _ : state)
    {
        registry.Update(0.016f);
        benchmark::DoNotOptimize(SumWorldDiagonal(registry));
    }
}

static void BM_Propagate_Animated(benchmark::State& state)
{
    const auto topology    = static_cast<Topology>(state.range(0));
    const auto probability = static_cast<float>(state.range(1)) / 100.f;
    const auto params      = ParamsFor(topology);
    auto       app         = CreateRegistry(params.targetNodes + 1'000);
    auto&      registry    = *app->registry;
    const auto tree        = SpawnTree(registry, params);
    std::mt19937 rng(123);

    for (auto _ : state)
    {
        for (const fr::Entity entity : tree.entities)
        {
            if (std::generate_canonical<float, 10>(rng) > probability)
                continue;
            registry.TryGetComponents<fr::LocalTransform3D>(
                entity, [&](fr::LocalTransform3D& local) { local.matrix[12] += 0.01f; });
            registry.MarkHierarchyDirty<fr::LocalTransform3D>(entity);
        }
        registry.Update(0.016f);
        benchmark::DoNotOptimize(SumWorldDiagonal(registry));
    }
}

static void BM_Propagate_Mode(benchmark::State& state)
{
    const auto topology = static_cast<Topology>(state.range(0));
    const auto mode     = state.range(1) == 0 ? fr::HierarchyPropagationMode::LevelSync
                                              : fr::HierarchyPropagationMode::WorkSharing;
    const auto params   = ParamsFor(topology);
    auto       app      = CreateRegistry(params.targetNodes + 1'000);
    auto&      registry = *app->registry;
    registry.GetHierarchyManager()->SetPropagationMode(mode);
    SpawnTree(registry, params);

    for (auto _ : state)
    {
        registry.Update(0.016f);
        benchmark::DoNotOptimize(SumWorldDiagonal(registry));
    }
}

static void BM_Propagate_ThreadScale(benchmark::State& state)
{
    const auto topology    = static_cast<Topology>(state.range(0));
    const auto threadCount = static_cast<std::uint64_t>(state.range(1));
    const auto mode        = state.range(2) == 0 ? fr::HierarchyPropagationMode::LevelSync
                                                 : fr::HierarchyPropagationMode::WorkSharing;
    const auto params      = ParamsFor(topology);
    auto       app         = CreateRegistry(params.targetNodes + 1'000, threadCount);
    auto&      registry    = *app->registry;
    registry.GetHierarchyManager()->SetPropagationMode(mode);
    SpawnTree(registry, params);

    state.SetLabel(std::to_string(params.targetNodes) + " ents / br=" +
                   std::to_string(params.branching) + " / depth<=" + std::to_string(params.depth));

    for (auto _ : state)
    {
        registry.Update(0.016f);
        benchmark::DoNotOptimize(SumWorldDiagonal(registry));
    }

    state.SetItemsProcessed(state.iterations() * params.targetNodes);
}

BENCHMARK(BM_Hierarchy_SetParent)->Arg(1'000)->Arg(10'000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Hierarchy_ChildrenIterate)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Hierarchy_CascadeDestroy)
    ->Arg(static_cast<int>(Topology::Wide))
    ->Arg(static_cast<int>(Topology::Deep))
    ->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Propagate_Static)
    ->Arg(static_cast<int>(Topology::Wide))
    ->Arg(static_cast<int>(Topology::Deep))
    ->Arg(static_cast<int>(Topology::Large))
    ->Arg(static_cast<int>(Topology::Huge))
    ->Arg(static_cast<int>(Topology::Massive))
    ->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Propagate_Animated)
    ->Args({ static_cast<int>(Topology::Large), 1 })
    ->Args({ static_cast<int>(Topology::Large), 50 })
    ->Args({ static_cast<int>(Topology::Large), 100 })
    ->Args({ static_cast<int>(Topology::Huge), 1 })
    ->Args({ static_cast<int>(Topology::Massive), 1 })
    ->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Propagate_Mode)
    ->Args({ static_cast<int>(Topology::Wide), 0 })
    ->Args({ static_cast<int>(Topology::Wide), 1 })
    ->Args({ static_cast<int>(Topology::Deep), 0 })
    ->Args({ static_cast<int>(Topology::Deep), 1 })
    ->Args({ static_cast<int>(Topology::Large), 0 })
    ->Args({ static_cast<int>(Topology::Large), 1 })
    ->Args({ static_cast<int>(Topology::Huge), 0 })
    ->Args({ static_cast<int>(Topology::Huge), 1 })
    ->Args({ static_cast<int>(Topology::Massive), 0 })
    ->Args({ static_cast<int>(Topology::Massive), 1 })
    ->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Propagate_ThreadScale)
    ->Args({ static_cast<int>(Topology::Large), 1, 1 })
    ->Args({ static_cast<int>(Topology::Large), 2, 1 })
    ->Args({ static_cast<int>(Topology::Large), 4, 1 })
    ->Args({ static_cast<int>(Topology::Large), 8, 1 })
    ->Args({ static_cast<int>(Topology::Huge), 1, 1 })
    ->Args({ static_cast<int>(Topology::Huge), 2, 1 })
    ->Args({ static_cast<int>(Topology::Huge), 4, 1 })
    ->Args({ static_cast<int>(Topology::Huge), 8, 1 })
    ->Args({ static_cast<int>(Topology::Massive), 1, 1 })
    ->Args({ static_cast<int>(Topology::Massive), 2, 1 })
    ->Args({ static_cast<int>(Topology::Massive), 4, 1 })
    ->Args({ static_cast<int>(Topology::Massive), 8, 1 })
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
