#include <Freyr/Freyr.hpp>
#include <gtest/gtest.h>

#include "../EmptyApp.hpp"

namespace
{
    struct PrefabHealth : fr::Component
    {
        float value = 0.f;
    };

    class PrefabCloneSpec : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            mApp = skr::ApplicationBuilder()
                       .WithExtension<fr::FreyrExtension>(
                           [](fr::FreyrExtension& freyr)
                           {
                               freyr.WithComponent<PrefabHealth>().WithOptions(
                                   [](fr::FreyrOptionsBuilder& options)
                                   { options.WithMaxEntities(256).WithThreadCount(2); });
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

TEST_F(PrefabCloneSpec, CloneShouldPreserveComponentValues)
{
    const auto source = mRegistry->CreateEntity(PrefabHealth {.value = 7.5f});
    mRegistry->ExecuteTasks();

    const auto clone = mRegistry->Clone(source);
    ASSERT_NE(clone, fr::NullEntity);
    ASSERT_TRUE(mRegistry->HasComponent<PrefabHealth>(clone));

    float value = 0.f;
    mRegistry->TryGetComponents<PrefabHealth>(clone, [&](PrefabHealth& health) { value = health.value; });
    EXPECT_FLOAT_EQ(value, 7.5f);
}

TEST_F(PrefabCloneSpec, PrefabShouldBeExcludedFromDefaultQuery)
{
    const auto prefab = mRegistry->CreateEntity(PrefabHealth {.value = 1.f}, fr::Prefab {});
    mRegistry->ExecuteTasks();

    EXPECT_EQ(mRegistry->CreateQuery()->Count<PrefabHealth>(), 0u);
    EXPECT_EQ(mRegistry->CreateQuery()->IncludingPrefabs()->Count<PrefabHealth>(), 1u);
    (void) prefab;
}

TEST_F(PrefabCloneSpec, InstantiateShouldAppearInDefaultQuery)
{
    const auto prefab = mRegistry->CreateEntity(PrefabHealth {.value = 3.f}, fr::Prefab {});
    mRegistry->ExecuteTasks();

    const auto instance = mRegistry->Instantiate(prefab);
    mRegistry->ExecuteTasks();

    ASSERT_NE(instance, fr::NullEntity);
    EXPECT_FALSE(mRegistry->HasComponent<fr::Prefab>(instance));
    EXPECT_TRUE(mRegistry->IsEnabled(instance));
    EXPECT_EQ(mRegistry->CreateQuery()->Count<PrefabHealth>(), 1u);
}

TEST_F(PrefabCloneSpec, CloneOfDisabledShouldRemainDisabled)
{
    const auto source = mRegistry->CreateEntity(PrefabHealth {.value = 1.f});
    mRegistry->ExecuteTasks();
    mRegistry->SetEnabled(source, false);
    mRegistry->ExecuteTasks();

    const auto clone = mRegistry->Clone(source);
    EXPECT_FALSE(mRegistry->IsEnabled(clone));
}
