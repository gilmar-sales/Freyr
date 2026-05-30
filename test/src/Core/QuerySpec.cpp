#include <gtest/gtest.h>

#include "Freyr/Core/FreyrExtension.hpp"
#include "Freyr/Core/Registry.hpp"

#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"

struct Velocity : fr::Component
{
    float x = 0.f;
    float y = 0.f;
};

class QueryApp : public skr::IApplication
{
  public:
    explicit QueryApp(const Ref<skr::ServiceProvider>& rootServiceProvider) : IApplication(rootServiceProvider) {}

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
                           .WithComponent<Velocity>();
                   })
                   .Build<QueryApp>();

        mRegistry = mApp->GetRootServiceProvider()->GetService<fr::Registry>();
    }

    Ref<QueryApp>  mApp;
    Ref<fr::Registry> mRegistry;
};

TEST_F(QuerySpec, QueryCountReturnsCorrectCount)
{
    mRegistry->CreateEntity<PositionComponent, Velocity>();
    mRegistry->CreateEntity<PositionComponent, Velocity>();
    mRegistry->CreateEntity<PositionComponent, Velocity>();

    const auto count = mRegistry->CreateQuery()->Count<PositionComponent, Velocity>();
    EXPECT_EQ(count, 3);
}

TEST_F(QuerySpec, QueryTransformReturnsVector)
{
    mRegistry->CreateEntity<PositionComponent>(PositionComponent { .x = 1.f, .y = 2.f });
    mRegistry->CreateEntity<PositionComponent>(PositionComponent { .x = 3.f, .y = 4.f });

    auto results = mRegistry->CreateQuery()->Transform<PositionComponent>([](fr::Entity e, PositionComponent& p) {
        return p.x + p.y;
    });

    EXPECT_EQ(results.size(), 2);
}

TEST_F(QuerySpec, QueryExcludingFiltersOutEntities)
{
    mRegistry->CreateEntity<PositionComponent>();
    mRegistry->CreateEntity<PositionComponent, Velocity>();

    auto count = mRegistry->CreateQuery()->Excluding<Velocity>().Count<PositionComponent>();
    EXPECT_EQ(count, 1);
}

TEST_F(QuerySpec, QueryReduceAggregatesValues)
{
    mRegistry->CreateEntity(Velocity { .x = 1.f, .y = 2.f });
    mRegistry->CreateEntity(Velocity { .x = 3.f, .y = 4.f });

    mRegistry->ExecuteTasks();

    const auto total =
        mRegistry->CreateQuery()->Reduce<Velocity>([](const float acc, Velocity& v) { return acc + v.x + v.y; }, 0.f);

    EXPECT_EQ(total, 10.f);
}

TEST_F(QuerySpec, Registry_Should_FindUnique)
{
    // Arrange
    for (auto i = 0; i < 2000; i++)
    {
        mRegistry->CreateEntity(PositionComponent {});
    }

    mRegistry->ExecuteTasks();

    constexpr auto modelEntity = static_cast<fr::Entity>(987);
    mRegistry->AddComponent(modelEntity, ModelComponent {});

    // Act
    const auto unique = mRegistry->CreateQuery()->FindUnique<PositionComponent, ModelComponent>();

    // Assert
    ASSERT_EQ(unique, modelEntity);
}
