#include <benchmark/benchmark.h>

#include <Freyr/Freyr.hpp>

#include <array>
#include <numeric>
#include <random>

struct Position : fr::Component {
    float x, y, z;
};

struct Velocity : fr::Component {
    float x, y, z;
};

struct Health : fr::Component {
    float value;
};

struct Damage : fr::Component {
    float value;
};

struct TagA : fr::Component {};
struct TagB : fr::Component {};
struct TagC : fr::Component {};
struct TagD : fr::Component {};
struct TagE : fr::Component {};

class BenchApp : public skr::IApplication {
public:
    explicit BenchApp(const skr::Arc<skr::ServiceProvider>& rootServiceProvider) :
        IApplication(rootServiceProvider) {
        registry = rootServiceProvider->GetService<fr::Registry>();
    }

    void Run() override {}

    skr::Arc<fr::Registry> registry;
};

static auto CreateRegistry(std::size_t maxEntities = 2'000'000, std::size_t chunkCapacity = 512) {
    return skr::ApplicationBuilder()
        .WithExtension<fr::FreyrExtension>([maxEntities, chunkCapacity](fr::FreyrExtension& freyr) {
            freyr
                .WithOptions([maxEntities, chunkCapacity](fr::FreyrOptionsBuilder& builder) {
                    builder.WithMaxEntities(maxEntities).WithArchetypeChunkCapacity(chunkCapacity).WithAllPhysicalCores();
                })
                .WithComponent<Position>()
                .WithComponent<Velocity>()
                .WithComponent<Health>()
                .WithComponent<Damage>()
                .WithComponent<TagA>()
                .WithComponent<TagB>()
                .WithComponent<TagC>()
                .WithComponent<TagD>()
                .WithComponent<TagE>();
        })
        .Build<BenchApp>();
}

static fr::Entity CreateEntityWithComponentCount(fr::Registry& registry, int compCount) {
    switch (compCount) {
        case 8:
            return registry.CreateEntity(
                Position{}, Velocity{}, Health{.value = 100.f}, Damage{.value = 10.f}, TagA{}, TagB{}, TagC{}, TagD{});
        case 7:
            return registry.CreateEntity(
                Position{}, Velocity{}, Health{.value = 100.f}, Damage{.value = 10.f}, TagA{}, TagB{}, TagC{});
        case 6:
            return registry.CreateEntity(
                Position{}, Velocity{}, Health{.value = 100.f}, Damage{.value = 10.f}, TagA{}, TagB{});
        case 5:
            return registry.CreateEntity(
                Position{}, Velocity{}, Health{.value = 100.f}, Damage{.value = 10.f}, TagA{});
        case 4:
            return registry.CreateEntity(Position{}, Velocity{}, Health{.value = 100.f}, Damage{.value = 10.f});
        case 3:
            return registry.CreateEntity(Position{}, Velocity{}, Health{.value = 100.f});
        case 2:
            return registry.CreateEntity(Position{}, Velocity{});
        default:
            return registry.CreateEntity(Position{});
    }
}

static void BM_EntityCreateDestroy(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    for (auto _ : state) {
        state.PauseTiming();
        std::vector<fr::Entity> entities;
        entities.reserve(entityCount);
        for (std::size_t i = 0; i < entityCount; ++i) {
            entities.push_back(registry.CreateEntity(Position{}, Velocity{}));
        }
        state.ResumeTiming();

        for (auto e : entities) {
            registry.DestroyEntity(e);
        }
        registry.ExecuteTasks();
    }

    state.SetItemsProcessed(state.iterations() * entityCount * 2);
}

static void BM_EntityCreateWithComponents(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));
    const auto compCount = static_cast<int>(state.range(1));

    for (auto _ : state) {
        state.PauseTiming();
        {
            auto app = CreateRegistry(entityCount + 1024);
            auto& registry = *app->registry;
            state.ResumeTiming();

            for (std::size_t i = 0; i < entityCount; ++i) {
                benchmark::DoNotOptimize(CreateEntityWithComponentCount(registry, compCount));
            }
            registry.ExecuteTasks();

            state.PauseTiming();
        }
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * entityCount);
}

static void BM_AddRemoveComponent_SingleArchetype(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));
    const auto iterations = static_cast<std::size_t>(state.range(1));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    std::vector<fr::Entity> entities;
    entities.reserve(entityCount);
    for (std::size_t i = 0; i < entityCount; ++i) {
        entities.push_back(registry.CreateEntity(Position{}, Velocity{}));
    }
    registry.ExecuteTasks();

    for (auto _ : state) {
        for (std::size_t iter = 0; iter < iterations; ++iter) {
            for (auto e : entities) {
                registry.AddComponent(e, Health{.value = 100.f});
                registry.RemoveComponent<Health>(e);
            }
        }
        registry.ExecuteTasks();
    }

    state.SetItemsProcessed(state.iterations() * entityCount * iterations * 2);
}

static void BM_AddRemoveComponent_MultiArchetype(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));
    const auto iterations = static_cast<std::size_t>(state.range(1));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    std::vector<fr::Entity> entities;
    entities.reserve(entityCount);
    for (std::size_t i = 0; i < entityCount; ++i) {
        entities.push_back(registry.CreateEntity(Position{}));
    }
    registry.ExecuteTasks();

    for (auto _ : state) {
        for (std::size_t iter = 0; iter < iterations; ++iter) {
            for (auto e : entities) {
                registry.AddComponent(e, Velocity{});
                registry.AddComponent(e, Health{.value = 100.f});
                registry.RemoveComponent<Velocity>(e);
                registry.RemoveComponent<Health>(e);
            }
        }
        registry.ExecuteTasks();
    }

    state.SetItemsProcessed(state.iterations() * entityCount * iterations * 4);
}

static void BM_Mutation_Each_Sync(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    registry.CreateArchetypeBuilder()
        .WithComponent(Position{})
        .WithComponent(Velocity{})
        .WithEntities(static_cast<fr::Entity>(entityCount))
        .Build();
    registry.ExecuteTasks();

    for (auto _ : state) {
        registry.CreateMutation()->Each(
            [](Position& pos, Velocity& vel) {
                pos.x += vel.x;
                pos.y += vel.y;
                pos.z += vel.z;
            });
    }

    state.SetItemsProcessed(state.iterations() * entityCount);
}

static void BM_Query_Count(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    registry.CreateArchetypeBuilder()
        .WithComponent(Position{})
        .WithComponent(Velocity{})
        .WithEntities(static_cast<fr::Entity>(entityCount))
        .Build();
    registry.ExecuteTasks();

    for (auto _ : state) {
        auto query = registry.CreateQuery();
        benchmark::DoNotOptimize(query->Count<Position, Velocity>());
    }

    state.SetItemsProcessed(state.iterations());
}

static void BM_Query_FindUnique(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    fr::Entity uniqueEntity = 0;
    for (std::size_t i = 0; i < entityCount; ++i) {
        auto e = registry.CreateEntity(Position{}, Velocity{}, Health{.value = 100.f});
        if (i == entityCount / 2) uniqueEntity = e;
    }
    registry.ExecuteTasks();

    for (auto _ : state) {
        auto query = registry.CreateQuery();
        benchmark::DoNotOptimize(query->FindUnique<Position, Velocity, Health>());
    }

    state.SetItemsProcessed(state.iterations());
}

static void BM_Query_EntitiesWith(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    registry.CreateArchetypeBuilder()
        .WithComponent(Position{})
        .WithComponent(Velocity{})
        .WithEntities(static_cast<fr::Entity>(entityCount))
        .Build();
    registry.ExecuteTasks();

    for (auto _ : state) {
        auto query = registry.CreateQuery();
        auto entities = query->EntitiesWith<Position, Velocity>();
        benchmark::DoNotOptimize(entities.size());
    }

    state.SetItemsProcessed(state.iterations() * entityCount);
}

static void BM_Query_Transform(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    registry.CreateArchetypeBuilder()
        .WithComponent(Position{})
        .WithComponent(Velocity{})
        .WithEntities(static_cast<fr::Entity>(entityCount))
        .Build();
    registry.ExecuteTasks();

    for (auto _ : state) {
        auto query = registry.CreateQuery();
        auto results = query->Transform([](Position& pos, Velocity& vel) {
            return pos.x + vel.x;
        });
        benchmark::DoNotOptimize(results.size());
    }

    state.SetItemsProcessed(state.iterations() * entityCount);
}

static void BM_Query_Map(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    registry.CreateArchetypeBuilder()
        .WithComponent(Position{})
        .WithComponent(Velocity{})
        .WithEntities(static_cast<fr::Entity>(entityCount))
        .Build();
    registry.ExecuteTasks();

    for (auto _ : state) {
        auto query = registry.CreateQuery();
        auto results = query->Map([](Position& pos, Velocity& vel) {
            return pos.x + vel.x;
        });
        benchmark::DoNotOptimize(results.size());
    }

    state.SetItemsProcessed(state.iterations() * entityCount);
}

static void BM_Query_Reduce(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    registry.CreateArchetypeBuilder()
        .WithComponent(Position{})
        .WithComponent(Velocity{})
        .WithEntities(static_cast<fr::Entity>(entityCount))
        .Build();
    registry.ExecuteTasks();

    for (auto _ : state) {
        auto query = registry.CreateQuery();
        float sum = query->Reduce([](float acc, Position& pos, Velocity& vel) {
            return acc + pos.x + vel.x;
        }, 0.0f);
        benchmark::DoNotOptimize(sum);
    }

    state.SetItemsProcessed(state.iterations() * entityCount);
}

static void BM_Query_FilterExclude(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    for (std::size_t i = 0; i < entityCount; ++i) {
        auto e = registry.CreateEntity(Position{}, Velocity{});
        if (i % 3 == 0) registry.AddComponent(e, TagA{});
        if (i % 5 == 0) registry.AddComponent(e, TagB{});
    }
    registry.ExecuteTasks();

    for (auto _ : state) {
        auto query = registry.CreateQuery();
        query->Excluding<TagA, TagB>();
        auto count = query->Count<Position, Velocity>();
        benchmark::DoNotOptimize(count);
    }

    state.SetItemsProcessed(state.iterations());
}

static void BM_Mutation_EachAsync(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));
    const auto mutationCount = static_cast<std::size_t>(state.range(1));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    registry.CreateArchetypeBuilder()
        .WithComponent(Position{})
        .WithComponent(Velocity{})
        .WithEntities(static_cast<fr::Entity>(entityCount))
        .Build();
    registry.ExecuteTasks();

    for (auto _ : state) {
        for (std::size_t i = 0; i < mutationCount; ++i) {
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

static void BM_Mutation_EachAsync_MultiArchetype(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));
    const auto mutationCount = static_cast<std::size_t>(state.range(1));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    registry.CreateArchetypeBuilder()
        .WithComponent(Position{})
        .WithComponent(Velocity{})
        .WithEntities(static_cast<fr::Entity>(entityCount / 2))
        .Build();
    registry.CreateArchetypeBuilder()
        .WithComponent(Position{})
        .WithComponent(Velocity{})
        .WithComponent(Health{})
        .WithEntities(static_cast<fr::Entity>(entityCount / 2))
        .Build();
    registry.ExecuteTasks();

    for (auto _ : state) {
        for (std::size_t i = 0; i < mutationCount; ++i) {
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

static void BM_Mutation_EachAsync_WithEntity(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));
    const auto mutationCount = static_cast<std::size_t>(state.range(1));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    registry.CreateArchetypeBuilder()
        .WithComponent(Position{})
        .WithComponent(Velocity{})
        .WithEntities(static_cast<fr::Entity>(entityCount))
        .Build();
    registry.ExecuteTasks();

    for (auto _ : state) {
        for (std::size_t i = 0; i < mutationCount; ++i) {
            registry.CreateMutation()->EachAsync(
                [](fr::Entity entity, Position& position, Velocity& velocity) {
                    position.x += velocity.x + static_cast<float>(entity);
                    position.y += velocity.y;
                    position.z += velocity.z;
                });
        }
        registry.ExecuteTasks();
    }

    state.SetItemsProcessed(state.iterations() * entityCount * mutationCount);
}

static void BM_Mutation_EachAsync_MultiComponent(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));
    const auto mutationCount = static_cast<std::size_t>(state.range(1));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    registry.CreateArchetypeBuilder()
        .WithComponent(Position{})
        .WithComponent(Velocity{})
        .WithComponent(Health{})
        .WithComponent(Damage{})
        .WithEntities(static_cast<fr::Entity>(entityCount))
        .Build();
    registry.ExecuteTasks();

    for (auto _ : state) {
        for (std::size_t i = 0; i < mutationCount; ++i) {
            registry.CreateMutation()->EachAsync(
                [](Position& pos, Velocity& vel, Health& hp, Damage& dmg) {
                    pos.x += vel.x;
                    hp.value -= dmg.value * 0.01f;
                });
        }
        registry.ExecuteTasks();
    }

    state.SetItemsProcessed(state.iterations() * entityCount * mutationCount);
}

static void BM_Mutation_EachAsync_FilteredByComponent(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));
    const auto mutationCount = static_cast<std::size_t>(state.range(1));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    for (std::size_t i = 0; i < entityCount; ++i) {
        auto e = registry.CreateEntity(Position{}, Velocity{});
        if (i % 2 == 0) registry.AddComponent(e, TagA{});
        if (i % 3 == 0) registry.AddComponent(e, TagB{});
    }
    registry.ExecuteTasks();

    for (auto _ : state) {
        for (std::size_t i = 0; i < mutationCount; ++i) {
            registry.CreateMutation()->EachAsync(
                [](Position& position, Velocity& velocity, TagA&) {
                    position.x += velocity.x;
                    position.y += velocity.y;
                    position.z += velocity.z;
                });
        }
        registry.ExecuteTasks();
    }

    state.SetItemsProcessed(state.iterations() * entityCount * mutationCount * 0.5);
}

static void BM_Mutation_Each_Sync_Exclude(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    registry.CreateArchetypeBuilder()
        .WithComponent(Position{})
        .WithComponent(Velocity{})
        .WithEntities(static_cast<fr::Entity>(entityCount))
        .Build();
    registry.ExecuteTasks();

    for (auto _ : state) {
        registry.CreateMutation()->Each(
            [](Position& pos, Velocity& vel) {
                pos.x += vel.x;
                pos.y += vel.y;
                pos.z += vel.z;
            });
    }

    state.SetItemsProcessed(state.iterations() * entityCount);
}

static void BM_Mutation_Each_ReuseQuery(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    registry.CreateArchetypeBuilder()
        .WithComponent(Position{})
        .WithComponent(Velocity{})
        .WithEntities(static_cast<fr::Entity>(entityCount))
        .Build();
    registry.ExecuteTasks();

    for (auto _ : state) {
        auto mutation = registry.CreateMutation();
        mutation->Each([](Position& pos, Velocity& vel) {
            pos.x += vel.x;
            pos.y += vel.y;
            pos.z += vel.z;
        });
    }

    state.SetItemsProcessed(state.iterations() * entityCount);
}

static void BM_ArchetypeMigration_AddComponent(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));
    const auto iterations = static_cast<std::size_t>(state.range(1));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    std::vector<fr::Entity> entities;
    entities.reserve(entityCount);
    for (std::size_t i = 0; i < entityCount; ++i) {
        entities.push_back(registry.CreateEntity(Position{}));
    }
    registry.ExecuteTasks();

    for (auto _ : state) {
        for (std::size_t iter = 0; iter < iterations; ++iter) {
            for (auto e : entities) {
                registry.AddComponent(e, Velocity{});
                registry.AddComponent(e, Health{.value = 100.f});
                registry.RemoveComponent<Velocity>(e);
                registry.RemoveComponent<Health>(e);
            }
        }
        registry.ExecuteTasks();
    }

    state.SetItemsProcessed(state.iterations() * entityCount * iterations * 4);
}

static void BM_ArchetypeMigration_TagToggle(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));
    const auto iterations = static_cast<std::size_t>(state.range(1));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    std::vector<fr::Entity> entities;
    entities.reserve(entityCount);
    for (std::size_t i = 0; i < entityCount; ++i) {
        entities.push_back(registry.CreateEntity(Position{}, Velocity{}));
    }
    registry.ExecuteTasks();

    for (auto _ : state) {
        for (std::size_t iter = 0; iter < iterations; ++iter) {
            for (std::size_t i = 0; i < entityCount; ++i) {
                if (i % 2 == 0) {
                    registry.AddComponent(entities[i], TagA{});
                    registry.RemoveComponent<TagA>(entities[i]);
                } else {
                    registry.AddComponent(entities[i], TagB{});
                    registry.RemoveComponent<TagB>(entities[i]);
                }
            }
        }
        registry.ExecuteTasks();
    }

    state.SetItemsProcessed(state.iterations() * entityCount * iterations * 2);
}

static void BM_MixedWorkload(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    auto app = CreateRegistry(entityCount + entityCount / 100 + 1024);
    auto& registry = *app->registry;

    for (std::size_t i = 0; i < entityCount / 4; ++i) {
        registry.CreateEntity(Position{}, Velocity{}, Health{.value = 100.f});
    }
    for (std::size_t i = 0; i < entityCount / 4; ++i) {
        registry.CreateEntity(Position{}, Velocity{}, Damage{.value = 10.f});
    }
    for (std::size_t i = 0; i < entityCount / 4; ++i) {
        auto e = registry.CreateEntity(Position{}, Velocity{});
        registry.AddComponent(e, TagA{});
    }
    for (std::size_t i = 0; i < entityCount / 4; ++i) {
        auto e = registry.CreateEntity(Position{}, Velocity{});
        registry.AddComponent(e, TagB{});
    }
    registry.ExecuteTasks();

    for (auto _ : state) {
        auto mutation = registry.CreateMutation();
        mutation->EachAsync([](Position& pos, Velocity& vel) {
            pos.x += vel.x * 0.5f;
        });
        registry.ExecuteTasks();

        for (std::size_t i = 0; i < entityCount / 100; ++i) {
            auto e = registry.CreateEntity(Position{}, Velocity{});
            registry.DestroyEntity(e);
        }
        registry.ExecuteTasks();

        std::vector<fr::Entity> spawned;
        spawned.reserve(entityCount / 100);
        for (std::size_t i = 0; i < entityCount / 100; ++i) {
            spawned.push_back(registry.CreateEntity(Position{}, Velocity{}, Health{.value = 100.f}));
        }
        registry.ExecuteTasks();

        state.PauseTiming();
        for (auto e : spawned) {
            registry.DestroyEntity(e);
        }
        registry.ExecuteTasks();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * entityCount * 3);
}

static void BM_Mutation_WithLabel_Profiling(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    auto app = CreateRegistry(entityCount + 1024);
    auto& registry = *app->registry;

    registry.CreateArchetypeBuilder()
        .WithComponent(Position{})
        .WithComponent(Velocity{})
        .WithEntities(static_cast<fr::Entity>(entityCount))
        .Build();
    registry.ExecuteTasks();

    for (auto _ : state) {
        auto mutation = registry.CreateMutation();
        mutation->WithLabel("PhysicsUpdate");
        mutation->Each([](Position& pos, Velocity& vel) {
            pos.x += vel.x;
        });
    }

    state.SetItemsProcessed(state.iterations() * entityCount);
}

static void BM_ArchetypeBuilder_BatchCreate(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();
        {
            auto app = CreateRegistry(entityCount + 1024);
            auto& registry = *app->registry;
            state.ResumeTiming();

            registry.CreateArchetypeBuilder()
                .WithComponent(Position{})
                .WithComponent(Velocity{})
                .WithEntities(static_cast<fr::Entity>(entityCount))
                .Build();
            registry.ExecuteTasks();

            state.PauseTiming();
        }
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * entityCount);
}

static void BM_ArchetypeBuilder_MultiArchetype(benchmark::State& state) {
    const auto entityCount = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();
        {
            auto app = CreateRegistry(entityCount + 1024);
            auto& registry = *app->registry;
            state.ResumeTiming();

            registry.CreateArchetypeBuilder()
                .WithComponent(Position{})
                .WithComponent(Velocity{})
                .WithEntities(static_cast<fr::Entity>(entityCount / 4))
                .Build();
            registry.CreateArchetypeBuilder()
                .WithComponent(Position{})
                .WithComponent(Velocity{})
                .WithComponent(Health{})
                .WithEntities(static_cast<fr::Entity>(entityCount / 4))
                .Build();
            registry.CreateArchetypeBuilder()
                .WithComponent(Position{})
                .WithComponent(Velocity{})
                .WithComponent(Damage{})
                .WithEntities(static_cast<fr::Entity>(entityCount / 4))
                .Build();
            registry.CreateArchetypeBuilder()
                .WithComponent(Position{})
                .WithComponent(Velocity{})
                .WithComponent(TagA{})
                .WithEntities(static_cast<fr::Entity>(entityCount / 4))
                .Build();
            registry.ExecuteTasks();

            state.PauseTiming();
        }
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * entityCount);
}

BENCHMARK(BM_EntityCreateDestroy)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_EntityCreateWithComponents)
    ->Args({10000, 1})
    ->Args({10000, 4})
    ->Args({10000, 8})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_AddRemoveComponent_SingleArchetype)
    ->Args({10000, 10})
    ->Args({10000, 100})
    ->Args({100000, 10})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_AddRemoveComponent_MultiArchetype)
    ->Args({10000, 10})
    ->Args({10000, 100})
    ->Args({100000, 10})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_Mutation_Each_Sync)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_Query_Count)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK(BM_Query_FindUnique)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK(BM_Query_EntitiesWith)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_Query_Transform)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_Query_Map)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_Query_Reduce)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_Query_FilterExclude)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_Mutation_EachAsync)
    ->Args({100000, 2})
    ->Args({100000, 4})
    ->Args({100000, 8})
    ->Args({100000, 16})
    ->Args({1000000, 8})
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Mutation_EachAsync_MultiArchetype)
    ->Args({100000, 2})
    ->Args({100000, 4})
    ->Args({100000, 8})
    ->Args({100000, 16})
    ->Args({1000000, 8})
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Mutation_EachAsync_WithEntity)
    ->Args({100000, 2})
    ->Args({100000, 4})
    ->Args({100000, 8})
    ->Args({100000, 16})
    ->Args({1000000, 8})
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Mutation_EachAsync_MultiComponent)
    ->Args({100000, 2})
    ->Args({100000, 4})
    ->Args({100000, 8})
    ->Args({100000, 16})
    ->Args({1000000, 8})
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Mutation_EachAsync_FilteredByComponent)
    ->Args({100000, 2})
    ->Args({100000, 4})
    ->Args({100000, 8})
    ->Args({100000, 16})
    ->Args({1000000, 8})
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Mutation_Each_Sync)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_Mutation_Each_Sync_Exclude)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_Mutation_Each_ReuseQuery)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_Mutation_WithLabel_Profiling)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_ArchetypeMigration_AddComponent)
    ->Args({10000, 10})
    ->Args({10000, 100})
    ->Args({100000, 10})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_ArchetypeMigration_TagToggle)
    ->Args({10000, 10})
    ->Args({10000, 100})
    ->Args({100000, 10})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_MixedWorkload)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_ArchetypeBuilder_BatchCreate)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_ArchetypeBuilder_MultiArchetype)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kMillisecond);