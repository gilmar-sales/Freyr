#include <gtest/gtest.h>

#include "Freyr/Core/FreyrExtension.hpp"
#include "Freyr/Core/Scene.hpp"

#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"

namespace FREYR_NAMESPACE
{
    struct Velocity : Component
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
                       .AddExtension<FreyrExtension>([](FreyrExtension& freyr) {
                           freyr.WithComponent<NameComponent>()
                               .WithComponent<PositionComponent>()
                               .WithComponent<ModelComponent>()
                               .WithComponent<Velocity>()
                               .WithOptions([](auto& builder) { builder.WithMaxEntities(1000); })
                               .WithPipeline([](auto& builder) {});
                       })
                       .Build<QueryApp>();

            mScene = mApp->GetRootServiceProvider().GetService<Scene>();
        }

        Ref<QueryApp> mApp;
        Ref<Scene>    mScene;
    };

    TEST_F(QuerySpec, QueryCountReturnsCorrectCount)
    {
        mScene->CreateEntity<PositionComponent, Velocity>();
        mScene->CreateEntity<PositionComponent, Velocity>();
        mScene->CreateEntity<PositionComponent, Velocity>();

        auto count = mScene->CreateQuery<PositionComponent, Velocity>()->Count();
        EXPECT_EQ(count, 3);
    }

    TEST_F(QuerySpec, QueryTransformReturnsVector)
    {
        mScene->CreateEntity<PositionComponent>(PositionComponent { .x = 1.f, .y = 2.f });
        mScene->CreateEntity<PositionComponent>(PositionComponent { .x = 3.f, .y = 4.f });

        auto results = mScene->CreateQuery()->Transform<PositionComponent>([](Entity e, PositionComponent& p) {
            return p.x + p.y;
        });

        EXPECT_EQ(results.size(), 2);
    }

    TEST_F(QuerySpec, QueryExcludingFiltersOutEntities)
    {
        mScene->CreateEntity<PositionComponent>();
        mScene->CreateEntity<PositionComponent, Velocity>();

        auto count = mScene->CreateQuery<PositionComponent>()->Excluding<Velocity>().Count();
        EXPECT_EQ(count, 1);
    }

    TEST_F(QuerySpec, QueryReduceAggregatesValues)
    {
        mScene->CreateEntity(Velocity { .x = 1.f, .y = 2.f });
        mScene->CreateEntity(Velocity { .x = 3.f, .y = 4.f });

        mScene->ExecuteTasks();

        const auto total =
            mScene->CreateQuery()->Reduce<Velocity>([](const float acc, Velocity& v) { return acc + v.x + v.y; }, 0.f);

        EXPECT_EQ(total, 10.f);
    }

} // namespace FREYR_NAMESPACE