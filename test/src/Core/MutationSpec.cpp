#include <gtest/gtest.h>

#include "Freyr/Core/FreyrExtension.hpp"
#include "Freyr/Core/Registry.hpp"

#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "../Components/VelocityComponent.hpp"

class MutationApp : public skr::IApplication
{
  public:
    explicit MutationApp(const skr::Arc<skr::ServiceProvider>& rootServiceProvider) :
        IApplication(rootServiceProvider)
    {
    }

    void Run() override {}
};

struct MutationSpec : public ::testing::Test
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
                   .Build<MutationApp>();

        mRegistry = mApp->GetRootServiceProvider()->GetService<fr::Registry>();
    }

    skr::Arc<MutationApp>  mApp;
    skr::Arc<fr::Registry> mRegistry;
};

TEST_F(MutationSpec, EachUpdatesMatchingEntitiesSynchronously)
{
    mRegistry->CreateEntity(PositionComponent { .x = 1.f, .y = 2.f });
    mRegistry->CreateEntity(PositionComponent { .x = 3.f, .y = 4.f });
    mRegistry->ExecuteTasks();

    mRegistry->CreateMutation()->Each<PositionComponent>([](fr::Entity, PositionComponent& position) {
        position.x += 10.f;
    });

    const auto xs = mRegistry->CreateQuery()->Transform<PositionComponent>(
        [](fr::Entity, PositionComponent& position) { return position.x; });

    EXPECT_EQ(xs.size(), 2);
    EXPECT_FLOAT_EQ(xs[0] + xs[1], 24.f);
}

TEST_F(MutationSpec, EachAsyncAppliesAfterExecuteTasks)
{
    mRegistry->CreateEntity(PositionComponent { .x = 1.f, .y = 0.f });
    mRegistry->CreateEntity(PositionComponent { .x = 2.f, .y = 0.f });

    mRegistry->CreateMutation()->EachAsync<PositionComponent>(
        [](fr::Entity, PositionComponent& position) { position.x *= 2.f; });

    mRegistry->ExecuteTasks();

    const auto total = mRegistry->CreateQuery()->Reduce<PositionComponent>(
        [](const float acc, PositionComponent& position) { return acc + position.x; }, 0.f);

    EXPECT_FLOAT_EQ(total, 6.f);
}

TEST_F(MutationSpec, EachSkipsArchetypesThatDoNotMatchFilter)
{
    mRegistry->CreateEntity(PositionComponent { .x = 5.f, .y = 0.f });
    mRegistry->CreateEntity(VelocityComponent { .x = 1.f, .y = 1.f });
    mRegistry->ExecuteTasks();

    mRegistry->CreateMutation()->Each<VelocityComponent>(
        [](fr::Entity, VelocityComponent& velocity) { velocity.x = 99.f; });

    const auto positionX = mRegistry->CreateQuery()->Reduce<PositionComponent>(
        [](const float acc, PositionComponent& position) { return acc + position.x; }, 0.f);
    const auto velocityX = mRegistry->CreateQuery()->Reduce<VelocityComponent>(
        [](const float acc, VelocityComponent& velocity) { return acc + velocity.x; }, 0.f);

    EXPECT_FLOAT_EQ(positionX, 5.f);
    EXPECT_FLOAT_EQ(velocityX, 99.f);
}

TEST_F(MutationSpec, EachAsyncWithNoMatchingArchetypesIsNoOp)
{
    mRegistry->CreateEntity(PositionComponent {});

    mRegistry->CreateMutation()->WithLabel("NoVelocity").EachAsync<VelocityComponent>(
        [](fr::Entity, VelocityComponent& velocity) { velocity.x = 42.f; });

    mRegistry->ExecuteTasks();

    EXPECT_EQ(mRegistry->CreateQuery()->Count<VelocityComponent>(), 0);
    EXPECT_EQ(mRegistry->CreateQuery()->Count<PositionComponent>(), 1);
}

TEST_F(MutationSpec, MultipleEachAsyncMutationsFlushTogether)
{
    mRegistry->CreateEntity(PositionComponent { .x = 1.f, .y = 0.f },
                            VelocityComponent { .x = 2.f, .y = 0.f });

    mRegistry->CreateMutation()->EachAsync<PositionComponent>(
        [](fr::Entity, PositionComponent& position) { position.x += 1.f; });
    mRegistry->CreateMutation()->EachAsync<VelocityComponent>(
        [](fr::Entity, VelocityComponent& velocity) { velocity.x += 1.f; });

    mRegistry->ExecuteTasks();

    const auto has = mRegistry->TryGetComponents<PositionComponent, VelocityComponent>(
        0, [](PositionComponent& position, VelocityComponent& velocity) {
            EXPECT_FLOAT_EQ(position.x, 2.f);
            EXPECT_FLOAT_EQ(velocity.x, 3.f);
        });

    EXPECT_TRUE(has);
}

TEST_F(MutationSpec, FlushOwnsPendingMutationsAcrossChunkTasks)
{
    mRegistry->CreateEntity(PositionComponent { .x = 1.f, .y = 0.f });
    mRegistry->CreateEntity(PositionComponent { .x = 2.f, .y = 0.f });

    mRegistry->CreateMutation()->EachAsync<PositionComponent>(
        [](fr::Entity, PositionComponent& position) { position.x *= 10.f; });

    mRegistry->ExecuteTasks();

    auto values = mRegistry->CreateQuery()->Map<PositionComponent>(
        [](fr::Entity, PositionComponent& position) { return position.x; });

    ASSERT_EQ(values.size(), 2);
    EXPECT_FLOAT_EQ(values[0] + values[1], 30.f);
}
