#include <algorithm>
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
    explicit QueryApp(const skr::Arc<skr::ServiceProvider>& rootServiceProvider) : IApplication(rootServiceProvider) {}

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

    skr::Arc<QueryApp>  mApp;
    skr::Arc<fr::Registry> mRegistry;
};

TEST_F(QuerySpec, QueryCountReturnsCorrectCount)
{
    mRegistry->CreateEntity<PositionComponent, VelocityComponent>();
    mRegistry->CreateEntity<PositionComponent, VelocityComponent>();
    mRegistry->CreateEntity<PositionComponent, VelocityComponent>();

    const auto count = mRegistry->CreateQuery()->Count<PositionComponent, VelocityComponent>();
    EXPECT_EQ(count, 3);
}

TEST_F(QuerySpec, QueryTransformReturnsVector)
{
    mRegistry->CreateEntity<PositionComponent>(PositionComponent { .x = 1.f, .y = 2.f });
    mRegistry->CreateEntity<PositionComponent>(PositionComponent { .x = 3.f, .y = 4.f });

    auto results = mRegistry->CreateQuery()->Transform([](fr::Entity e, PositionComponent& p) {
        return p.x + p.y;
    });

    EXPECT_EQ(results.size(), 2);
}

TEST_F(QuerySpec, QueryExcludingFiltersOutEntities)
{
    mRegistry->CreateEntity<PositionComponent>();
    mRegistry->CreateEntity<PositionComponent, VelocityComponent>();

    auto count = mRegistry->CreateQuery()->Excluding<VelocityComponent>().Count<PositionComponent>();
    EXPECT_EQ(count, 1);
}

TEST_F(QuerySpec, QueryReduceAggregatesValues)
{
    mRegistry->CreateEntity(VelocityComponent { .x = 1.f, .y = 2.f });
    mRegistry->CreateEntity(VelocityComponent { .x = 3.f, .y = 4.f });

    mRegistry->ExecuteTasks();

    const auto total =
        mRegistry->CreateQuery()->Reduce([](const float acc, VelocityComponent& v) { return acc + v.x + v.y; }, 0.f);

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

    auto results = mRegistry->CreateQuery()->Transform(
        [](PositionComponent& position) { return position.x + position.y; });

    EXPECT_EQ(results.size(), 2);
    EXPECT_FLOAT_EQ(results[0] + results[1], 10.f);
}

TEST_F(QuerySpec, QueryMapReturnsTransformedValues)
{
    mRegistry->CreateEntity(PositionComponent { .x = 1.f, .y = 0.f });
    mRegistry->CreateEntity(PositionComponent { .x = 2.f, .y = 0.f });
    mRegistry->CreateEntity(PositionComponent { .x = 3.f, .y = 0.f });
    mRegistry->ExecuteTasks();

    auto results = mRegistry->CreateQuery()->Map(
        [](fr::Entity, PositionComponent& position) { return position.x; });

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

    const auto first = mRegistry->CreateQuery()->First<PositionComponent>();

    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, expected);
}

TEST_F(QuerySpec, QueryFirstReturnsNulloptWhenEmpty)
{
    mRegistry->CreateEntity(VelocityComponent {});

    const auto first = mRegistry->CreateQuery()->First<PositionComponent>();

    EXPECT_FALSE(first.has_value());
}

TEST_F(QuerySpec, QueryEntitiesWithReturnsAllMatchingEntities)
{
    const auto a = mRegistry->CreateEntity(PositionComponent {}, VelocityComponent {});
    mRegistry->CreateEntity(PositionComponent {});
    const auto b = mRegistry->CreateEntity(PositionComponent {}, VelocityComponent {});

    auto entities = mRegistry->CreateQuery()->EntitiesWith<PositionComponent, VelocityComponent>();

    ASSERT_EQ(entities.size(), 2);
    EXPECT_NE(std::find(entities.begin(), entities.end(), a), entities.end());
    EXPECT_NE(std::find(entities.begin(), entities.end(), b), entities.end());
}

TEST_F(QuerySpec, FindUniqueReturnsNulloptWhenMultipleInSameArchetype)
{
    mRegistry->CreateEntity(PositionComponent {}, ModelComponent {});
    mRegistry->CreateEntity(PositionComponent {}, ModelComponent {});

    const auto unique = mRegistry->CreateQuery()->FindUnique<PositionComponent, ModelComponent>();

    EXPECT_FALSE(unique.has_value());
}

TEST_F(QuerySpec, FindUniqueReturnsNulloptWhenMultipleMatchingArchetypes)
{
    mRegistry->CreateEntity(PositionComponent {}, ModelComponent {});
    mRegistry->CreateEntity(PositionComponent {}, ModelComponent {}, VelocityComponent {});

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

    const auto count =
        mRegistry->CreateQuery()->WithLabel("positions").Count<PositionComponent>();

    EXPECT_EQ(count, 1);
}
