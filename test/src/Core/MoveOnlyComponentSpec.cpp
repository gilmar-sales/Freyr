#include <Freyr/Freyr.hpp>
#include <gtest/gtest.h>

#include "../Components/PositionComponent.hpp"
#include "../Components/VelocityComponent.hpp"
#include "../EmptyApp.hpp"

#include <memory>

namespace
{
struct MoveOnlyHolder
{
    std::unique_ptr<int> value;
};

class MoveOnlyComponentSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mApp = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>(
                       [](fr::FreyrExtension& freyr)
                       {
                           freyr.WithComponent<MoveOnlyHolder>()
                               .WithComponent<PositionComponent>()
                               .WithComponent<VelocityComponent>();
                       })
                   .Build<EmptyApp>();
        mRegistry = mApp->GetRootServiceProvider()->GetService<fr::Registry>();
    }

    void TearDown() override
    {
        mRegistry.reset();
        mApp.reset();
    }

    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<EmptyApp>     mApp;
};
} // namespace

TEST_F(MoveOnlyComponentSpec, MigrationShouldPreserveMoveOnlyValue)
{
    MoveOnlyHolder holder;
    holder.value = std::make_unique<int>(42);
    const auto entity =
        mRegistry->CreateEntity(std::move(holder), PositionComponent {.x = 1.f});
    mRegistry->ExecuteTasks();

    ASSERT_TRUE((mRegistry->HasComponents<MoveOnlyHolder, PositionComponent>(entity)));

    mRegistry->AddComponent(entity, VelocityComponent {.x = 2.f});
    mRegistry->ExecuteTasks();

    bool visited = false;
    ASSERT_TRUE(mRegistry->TryGetComponents<MoveOnlyHolder>(
        entity,
        [&](MoveOnlyHolder& actual)
        {
            ASSERT_TRUE(actual.value);
            EXPECT_EQ(*actual.value, 42);
            visited = true;
        }));
    ASSERT_TRUE(visited);

    mRegistry->RemoveComponent<PositionComponent>(entity);
    mRegistry->ExecuteTasks();

    visited = false;
    ASSERT_TRUE(mRegistry->TryGetComponents<MoveOnlyHolder>(
        entity,
        [&](MoveOnlyHolder& actual)
        {
            ASSERT_TRUE(actual.value);
            EXPECT_EQ(*actual.value, 42);
            visited = true;
        }));
    ASSERT_TRUE(visited);
}
