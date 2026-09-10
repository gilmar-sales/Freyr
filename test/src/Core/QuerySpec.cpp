#include <algorithm>
#include <atomic>
#include <gtest/gtest.h>

#include "Freyr/Core/FreyrExtension.hpp"
#include "Freyr/Core/Registry.hpp"

#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "../Components/VelocityComponent.hpp"

class QueryApp : public skr::IApplication
{
  public:
    explicit QueryApp(const skr::Arc<skr::ServiceProvider>& rootServiceProvider) :
        IApplication(rootServiceProvider)
    {
    }

    void Run() override {}
};

struct QuerySpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mApp = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<NameComponent>()
                           .WithComponent<PositionComponent>()
                           .WithComponent<ModelComponent>()
                           .WithComponent<VelocityComponent>();
                   })
                   .Build<QueryApp>();

        mRegistry = mApp->GetRootServiceProvider()->GetService<fr::Registry>();
    }

    skr::Arc<QueryApp>     mApp;
    skr::Arc<fr::Registry> mRegistry;
};

TEST_F(QuerySpec, QueryCountReturnsCorrectCount)
{
    mRegistry->CreateEntity<PositionComponent, VelocityComponent>();
    mRegistry->CreateEntity<PositionComponent, VelocityComponent>();
    mRegistry->CreateEntity<PositionComponent, VelocityComponent>();
    mRegistry->ExecuteTasks();

    const auto count = mRegistry->CreateQuery()->Count<PositionComponent, VelocityComponent>();
    EXPECT_EQ(count, 3);
}

TEST_F(QuerySpec, QueryTransformReturnsVector)
{
    mRegistry->CreateEntity<PositionComponent>(PositionComponent { .x = 1.f, .y = 2.f });
    mRegistry->CreateEntity<PositionComponent>(PositionComponent { .x = 3.f, .y = 4.f });
    mRegistry->ExecuteTasks();

    auto results = mRegistry->CreateQuery()->Transform([](fr::Entity e, PositionComponent& p) {
        return p.x + p.y;
    });

    EXPECT_EQ(results.size(), 2);
}

TEST_F(QuerySpec, QueryExcludingFiltersOutEntities)
{
    mRegistry->CreateEntity<PositionComponent>();
    mRegistry->CreateEntity<PositionComponent, VelocityComponent>();
    mRegistry->ExecuteTasks();

    auto count =
        mRegistry->CreateQuery()->Excluding<VelocityComponent>().Count<PositionComponent>();
    EXPECT_EQ(count, 1);
}

TEST_F(QuerySpec, QueryReduceAggregatesValues)
{
    mRegistry->CreateEntity(VelocityComponent { .x = 1.f, .y = 2.f });
    mRegistry->CreateEntity(VelocityComponent { .x = 3.f, .y = 4.f });

    mRegistry->ExecuteTasks();

    const auto total = mRegistry->CreateQuery()->Reduce(
        [](const float acc, VelocityComponent& v) { return acc + v.x + v.y; }, 0.f);

    EXPECT_EQ(total, 10.f);
}

TEST_F(QuerySpec, Registry_Should_FindUnique)
{
    // Arrange
    for (auto i = 0; i < 2000; i++)
    {
        mRegistry->CreateEntity(PositionComponent {});
    }

    constexpr auto modelEntity = static_cast<fr::Entity>(987);
    mRegistry->AddComponent(modelEntity, ModelComponent {});

    mRegistry->ExecuteTasks();

    // Act
    const auto unique = mRegistry->CreateQuery()->FindUnique<PositionComponent, ModelComponent>();

    // Assert
    ASSERT_EQ(unique, modelEntity);
}

TEST_F(QuerySpec, QueryTransformWithoutEntityReturnsValues)
{
    mRegistry->CreateEntity(PositionComponent { .x = 1.f, .y = 2.f });
    mRegistry->CreateEntity(PositionComponent { .x = 3.f, .y = 4.f });
    mRegistry->ExecuteTasks();

    auto results = mRegistry->CreateQuery()->Transform([](PositionComponent& position) {
        return position.x + position.y;
    });

    EXPECT_EQ(results.size(), 2);
    EXPECT_FLOAT_EQ(results[0] + results[1], 10.f);
}

TEST_F(QuerySpec, QueryMapReturnsTransformedValues)
{
    mRegistry->CreateEntity(PositionComponent { .x = 1.f, .y = 0.f });
    mRegistry->CreateEntity(PositionComponent { .x = 2.f, .y = 0.f });
    mRegistry->CreateEntity(PositionComponent { .x = 3.f, .y = 0.f });
    mRegistry->ExecuteTasks();

    auto results = mRegistry->CreateQuery()->Map([](fr::Entity, PositionComponent& position) {
        return position.x;
    });

    EXPECT_EQ(results.size(), 3);
    EXPECT_FLOAT_EQ(results[0] + results[1] + results[2], 6.f);
}

TEST_F(QuerySpec, QueryIterateReturnsEntityComponentTuples)
{
    const auto a = mRegistry->CreateEntity(PositionComponent { .x = 10.f, .y = 0.f });
    const auto b = mRegistry->CreateEntity(PositionComponent { .x = 20.f, .y = 0.f });
    mRegistry->ExecuteTasks();

    auto results = mRegistry->CreateQuery()->Iterate<PositionComponent>();

    ASSERT_EQ(results.size(), 2);

    std::vector<fr::Entity> entities;
    float                   totalX = 0.f;
    for (auto& [entity, position] : results)
    {
        entities.push_back(entity);
        totalX += position.x;
    }

    EXPECT_FLOAT_EQ(totalX, 30.f);
    EXPECT_NE(std::find(entities.begin(), entities.end(), a), entities.end());
    EXPECT_NE(std::find(entities.begin(), entities.end(), b), entities.end());
}

TEST_F(QuerySpec, QueryFirstReturnsMatchingEntity)
{
    mRegistry->CreateEntity(VelocityComponent {});
    const auto expected = mRegistry->CreateEntity(PositionComponent { .x = 7.f, .y = 0.f });
    mRegistry->ExecuteTasks();

    const auto first = mRegistry->CreateQuery()->First<PositionComponent>();

    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, expected);
}

TEST_F(QuerySpec, QueryFirstReturnsNulloptWhenEmpty)
{
    mRegistry->CreateEntity(VelocityComponent {});
    mRegistry->ExecuteTasks();

    const auto first = mRegistry->CreateQuery()->First<PositionComponent>();

    EXPECT_FALSE(first.has_value());
}

TEST_F(QuerySpec, QueryEntitiesWithReturnsAllMatchingEntities)
{
    const auto a = mRegistry->CreateEntity(PositionComponent {}, VelocityComponent {});
    mRegistry->CreateEntity(PositionComponent {});
    const auto b = mRegistry->CreateEntity(PositionComponent {}, VelocityComponent {});
    mRegistry->ExecuteTasks();

    auto entities = mRegistry->CreateQuery()->EntitiesWith<PositionComponent, VelocityComponent>();

    ASSERT_EQ(entities.size(), 2);
    EXPECT_NE(std::find(entities.begin(), entities.end(), a), entities.end());
    EXPECT_NE(std::find(entities.begin(), entities.end(), b), entities.end());
}

TEST_F(QuerySpec, FindUniqueReturnsNulloptWhenMultipleInSameArchetype)
{
    mRegistry->CreateEntity(PositionComponent {}, ModelComponent {});
    mRegistry->CreateEntity(PositionComponent {}, ModelComponent {});
    mRegistry->ExecuteTasks();

    const auto unique = mRegistry->CreateQuery()->FindUnique<PositionComponent, ModelComponent>();

    EXPECT_FALSE(unique.has_value());
}

TEST_F(QuerySpec, FindUniqueReturnsNulloptWhenMultipleMatchingArchetypes)
{
    mRegistry->CreateEntity(PositionComponent {}, ModelComponent {});
    mRegistry->CreateEntity(PositionComponent {}, ModelComponent {}, VelocityComponent {});
    mRegistry->ExecuteTasks();

    const auto unique = mRegistry->CreateQuery()->FindUnique<PositionComponent, ModelComponent>();

    EXPECT_FALSE(unique.has_value());
}

TEST_F(QuerySpec, FindUniqueSkipsEmptyMatchingArchetypes)
{
    const auto entity = mRegistry->CreateEntity(PositionComponent {}, ModelComponent {});
    mRegistry->AddComponent(entity, VelocityComponent {});
    mRegistry->ExecuteTasks();

    const auto unique = mRegistry->CreateQuery()->FindUnique<PositionComponent, ModelComponent>();

    ASSERT_TRUE(unique.has_value());
    EXPECT_EQ(*unique, entity);
}

TEST_F(QuerySpec, QueryWithLabelCanBeChained)
{
    mRegistry->CreateEntity(PositionComponent {});
    mRegistry->ExecuteTasks();

    const auto count = mRegistry->CreateQuery()->WithLabel("positions").Count<PositionComponent>();

    EXPECT_EQ(count, 1);
}

TEST_F(QuerySpec, QueryMapShouldCollectResultsInSinglePass)
{
    mRegistry->CreateEntity(PositionComponent { .x = 1.f, .y = 0.f });
    mRegistry->CreateEntity(PositionComponent { .x = 2.f, .y = 0.f });
    mRegistry->CreateEntity(PositionComponent { .x = 3.f, .y = 0.f });
    mRegistry->ExecuteTasks();

    const auto values =
        mRegistry->CreateQuery()->Map([](PositionComponent& position) { return position.x; });

    ASSERT_EQ(values.size(), 3u);
    EXPECT_FLOAT_EQ(values[0] + values[1] + values[2], 6.f);
}

TEST_F(QuerySpec, ForEachChunkExposesContiguousComponentColumns)
{
    mRegistry->CreateEntity(PositionComponent { .x = 1.f, .y = 0.f, .z = 0.f });
    mRegistry->CreateEntity(PositionComponent { .x = 2.f, .y = 0.f, .z = 0.f });
    mRegistry->CreateEntity(PositionComponent { .x = 3.f, .y = 0.f, .z = 0.f });
    mRegistry->ExecuteTasks();

    std::size_t entityTotal = 0;
    float       sumX        = 0.f;

    mRegistry->CreateQuery()->ForEachChunk<PositionComponent>([&](fr::ChunkView view) {
        const auto positions = view.Column<PositionComponent>();
        const auto entities  = view.Entities();

        EXPECT_EQ(positions.size(), entities.size());
        EXPECT_FALSE(positions.empty());

        for (std::size_t i = 1; i < positions.size(); ++i)
        {
            EXPECT_EQ(&positions[i] - &positions[i - 1], 1);
            EXPECT_EQ(&entities[i] - &entities[i - 1], 1);
        }

        for (std::size_t i = 0; i < positions.size(); ++i)
        {
            sumX += positions[i].x;
            ++entityTotal;
        }
    });

    EXPECT_EQ(entityTotal, 3u);
    EXPECT_FLOAT_EQ(sumX, 6.f);
    EXPECT_EQ(mRegistry->CreateQuery()->Count<PositionComponent>(), entityTotal);
}

TEST_F(QuerySpec, ForEachChunkAlignsMultipleColumnsWithEntities)
{
    const auto a =
        mRegistry->CreateEntity(PositionComponent { .x = 10.f }, VelocityComponent { .x = 1.f });
    const auto b =
        mRegistry->CreateEntity(PositionComponent { .x = 20.f }, VelocityComponent { .x = 2.f });
    mRegistry->ExecuteTasks();

    std::size_t matched = 0;

    mRegistry->CreateQuery()->ForEachChunk<PositionComponent, VelocityComponent>(
        [&](fr::ChunkView view) {
            const auto positions = view.Column<PositionComponent>();
            const auto velocities = view.Column<VelocityComponent>();
            const auto entities   = view.Entities();

            EXPECT_EQ(positions.size(), velocities.size());
            EXPECT_EQ(positions.size(), entities.size());

            for (std::size_t i = 0; i < entities.size(); ++i)
            {
                if (entities[i] == a)
                {
                    EXPECT_FLOAT_EQ(positions[i].x, 10.f);
                    EXPECT_FLOAT_EQ(velocities[i].x, 1.f);
                    ++matched;
                }
                else if (entities[i] == b)
                {
                    EXPECT_FLOAT_EQ(positions[i].x, 20.f);
                    EXPECT_FLOAT_EQ(velocities[i].x, 2.f);
                    ++matched;
                }
            }
        });

    EXPECT_EQ(matched, 2u);
}

TEST_F(QuerySpec, ForEachChunkRespectsExcludingFilter)
{
    mRegistry->CreateEntity(PositionComponent { .x = 1.f });
    mRegistry->CreateEntity(PositionComponent { .x = 2.f }, VelocityComponent {});
    mRegistry->CreateEntity(PositionComponent { .x = 3.f });
    mRegistry->ExecuteTasks();

    std::size_t count = 0;
    float       sumX  = 0.f;

    mRegistry->CreateQuery()->Excluding<VelocityComponent>().ForEachChunk<PositionComponent>(
        [&](fr::ChunkView view) {
            for (const auto& position : view.Column<PositionComponent>())
            {
                sumX += position.x;
                ++count;
            }
        });

    EXPECT_EQ(count, 2u);
    EXPECT_FLOAT_EQ(sumX, 4.f);
}

struct QueryChunkCapacitySpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mApp = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>()
                           .WithComponent<VelocityComponent>()
                           .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                               builder.WithArchetypeChunkCapacity(4).WithMaxEntities(64);
                           });
                   })
                   .Build<QueryApp>();

        mRegistry = mApp->GetRootServiceProvider()->GetService<fr::Registry>();
    }

    skr::Arc<QueryApp>     mApp;
    skr::Arc<fr::Registry> mRegistry;
};

TEST_F(QueryChunkCapacitySpec, ForEachChunkVisitsMultipleChunksWithStableCounts)
{
    constexpr std::size_t entityCount = 10;
    for (std::size_t i = 0; i < entityCount; ++i)
    {
        mRegistry->CreateEntity(PositionComponent { .x = static_cast<float>(i) });
    }
    mRegistry->ExecuteTasks();

    std::size_t chunkVisits = 0;
    std::size_t total       = 0;
    float       sumX        = 0.f;

    mRegistry->CreateQuery()->ForEachChunk<PositionComponent>([&](fr::ChunkView view) {
        ++chunkVisits;
        EXPECT_LE(view.size(), 4u);
        total += view.size();
        for (const auto& position : view.Column<PositionComponent>())
            sumX += position.x;
    });

    EXPECT_GT(chunkVisits, 1u);
    EXPECT_EQ(total, entityCount);
    EXPECT_EQ(mRegistry->CreateQuery()->Count<PositionComponent>(), total);
    EXPECT_FLOAT_EQ(sumX, 45.f);
}

TEST_F(QueryChunkCapacitySpec, ForEachChunkAsyncRunsAfterExecuteTasks)
{
    constexpr std::size_t entityCount = 10;
    for (std::size_t i = 0; i < entityCount; ++i)
    {
        mRegistry->CreateEntity(PositionComponent { .x = static_cast<float>(i) });
    }
    mRegistry->ExecuteTasks();

    std::atomic<std::size_t> chunkVisits { 0 };
    std::atomic<std::size_t> total { 0 };
    std::atomic<float>       sumX { 0.f };

    mRegistry->CreateQuery()->WithLabel("ExtractAsync").ForEachChunkAsync<PositionComponent>(
        [&](fr::ChunkView view) {
            chunkVisits.fetch_add(1, std::memory_order_relaxed);
            total.fetch_add(view.size(), std::memory_order_relaxed);

            float local = 0.f;
            for (const auto& position : view.Column<PositionComponent>())
                local += position.x;

            float previous = sumX.load(std::memory_order_relaxed);
            while (!sumX.compare_exchange_weak(previous,
                                               previous + local,
                                               std::memory_order_relaxed))
            {
            }
        });

    EXPECT_EQ(chunkVisits.load(), 0u);
    EXPECT_EQ(total.load(), 0u);

    mRegistry->ExecuteTasks();

    EXPECT_GT(chunkVisits.load(), 1u);
    EXPECT_EQ(total.load(), entityCount);
    EXPECT_FLOAT_EQ(sumX.load(), 45.f);
}

TEST_F(QueryChunkCapacitySpec, ForEachChunkAsyncRespectsExcludingFilter)
{
    for (std::size_t i = 0; i < 8; ++i)
    {
        if (i % 2 == 0)
            mRegistry->CreateEntity(PositionComponent { .x = 1.f });
        else
            mRegistry->CreateEntity(PositionComponent { .x = 1.f }, VelocityComponent {});
    }
    mRegistry->ExecuteTasks();

    std::atomic<std::size_t> total { 0 };

    mRegistry->CreateQuery()
        ->Excluding<VelocityComponent>()
        .ForEachChunkAsync<PositionComponent>([&](fr::ChunkView view) {
            total.fetch_add(view.size(), std::memory_order_relaxed);
        });

    mRegistry->ExecuteTasks();

    EXPECT_EQ(total.load(), 4u);
}
