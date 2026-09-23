#include <Freyr/Freyr.hpp>
#include <gtest/gtest.h>

#include "../EmptyApp.hpp"

namespace
{
    struct ChangeHealth : fr::Component
    {
        float value = 0.f;
    };

    class ChangeDetectionSpec : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            mApp = skr::ApplicationBuilder()
                       .WithExtension<fr::FreyrExtension>(
                           [](fr::FreyrExtension& freyr)
                           {
                               freyr.WithComponent<ChangeHealth>().WithOptions(
                                   [](fr::FreyrOptionsBuilder& options) {
                                       options.WithMaxEntities(256).WithThreadCount(2);
                                   });
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

TEST_F(ChangeDetectionSpec, AddedShouldMatchEntitiesCreatedThisTick)
{
    mRegistry->Update(0.016f);
    mRegistry->CreateEntity(ChangeHealth {.value = 1.f});
    mRegistry->ExecuteTasks();

    EXPECT_EQ(mRegistry->CreateQuery()->Added<ChangeHealth>().Count<ChangeHealth>(), 1u);

    mRegistry->Update(0.016f);
    EXPECT_EQ(mRegistry->CreateQuery()->Added<ChangeHealth>().Count<ChangeHealth>(), 0u);
}

TEST_F(ChangeDetectionSpec, ChangedShouldMatchMutatedEntities)
{
    const auto entity = mRegistry->CreateEntity(ChangeHealth {.value = 1.f});
    mRegistry->ExecuteTasks();
    mRegistry->Update(0.016f);

    EXPECT_EQ(mRegistry->CreateQuery()->Changed<ChangeHealth>().Count<ChangeHealth>(), 0u);

    mRegistry->CreateMutation()->Each(
        [](ChangeHealth& health) { health.value = 2.f; });
    EXPECT_EQ(mRegistry->CreateQuery()->Changed<ChangeHealth>().Count<ChangeHealth>(), 1u);
    (void) entity;
}

TEST_F(ChangeDetectionSpec, ObserveAddAndRemoveShouldFireOnFlush)
{
    std::vector<fr::Entity>       added;
    std::vector<fr::EntityHandle> removed;

    mRegistry->ObserveAdd<ChangeHealth>([&](fr::Entity e) { added.push_back(e); });
    mRegistry->ObserveRemove<ChangeHealth>([&](fr::EntityHandle h) { removed.push_back(h); });

    const auto entity = mRegistry->CreateEntity(ChangeHealth {.value = 1.f});
    mRegistry->ExecuteTasks();
    ASSERT_EQ(added.size(), 1u);
    EXPECT_EQ(added[0], entity);

    mRegistry->RemoveComponent<ChangeHealth>(entity);
    mRegistry->ExecuteTasks();
    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed[0].entity, entity);
}

TEST_F(ChangeDetectionSpec, RemovedShouldMatchAfterNextTick)
{
    const auto entity = mRegistry->CreateEntity(ChangeHealth {.value = 1.f});
    mRegistry->ExecuteTasks();

    mRegistry->RemoveComponent<ChangeHealth>(entity);
    mRegistry->ExecuteTasks();

    EXPECT_EQ(mRegistry->CreateQuery()->CountRemoved<ChangeHealth>(), 0u);

    mRegistry->Update(0.016f);
    EXPECT_EQ(mRegistry->CreateQuery()->CountRemoved<ChangeHealth>(), 1u);

    std::size_t seen = 0;
    mRegistry->CreateQuery()->ForEachRemoved<ChangeHealth>([&](fr::EntityHandle) { ++seen; });
    EXPECT_EQ(seen, 1u);
}

TEST_F(ChangeDetectionSpec, ReAddingExistingComponentShouldOverwriteMarkAddedAndNotifyObservers)
{
    std::vector<fr::Entity> added;
    mRegistry->ObserveAdd<ChangeHealth>([&](fr::Entity e) { added.push_back(e); });

    const auto entity = mRegistry->CreateEntity(ChangeHealth {.value = 1.f});
    mRegistry->ExecuteTasks();
    mRegistry->Update(0.016f);
    added.clear();
    const auto archetypes = mRegistry->ArchetypeCount();

    mRegistry->AddComponent(entity, ChangeHealth {.value = 7.f});
    mRegistry->ExecuteTasks();

    float value = 0.f;
    ASSERT_TRUE(mRegistry->TryGetComponents<ChangeHealth>(
        entity, [&](ChangeHealth& health) { value = health.value; }));
    EXPECT_FLOAT_EQ(value, 7.f);
    EXPECT_EQ(mRegistry->CreateQuery()->Added<ChangeHealth>().Count<ChangeHealth>(), 1u);
    EXPECT_EQ(mRegistry->CreateQuery()->Changed<ChangeHealth>().Count<ChangeHealth>(), 1u);
    ASSERT_EQ(added.size(), 1u);
    EXPECT_EQ(added[0], entity);
    EXPECT_EQ(mRegistry->ArchetypeCount(), archetypes);
}
