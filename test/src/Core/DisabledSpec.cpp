#include <Freyr/Freyr.hpp>
#include <gtest/gtest.h>

#include "../EmptyApp.hpp"

namespace
{
    struct DisabledPos : fr::Component
    {
        float x = 0.f;
    };

    class DisabledSpec : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            mApp = skr::ApplicationBuilder()
                       .WithExtension<fr::FreyrExtension>(
                           [](fr::FreyrExtension& freyr)
                           {
                               freyr.WithComponent<DisabledPos>().WithOptions(
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

TEST_F(DisabledSpec, DisabledEntityShouldBeExcludedFromDefaultQuery)
{
    const auto a = mRegistry->CreateEntity(DisabledPos {.x = 1.f});
    const auto b = mRegistry->CreateEntity(DisabledPos {.x = 2.f});
    mRegistry->ExecuteTasks();

    mRegistry->SetEnabled(b, false);
    mRegistry->ExecuteTasks();

    EXPECT_TRUE(mRegistry->IsEnabled(a));
    EXPECT_FALSE(mRegistry->IsEnabled(b));
    EXPECT_EQ(mRegistry->CreateQuery()->Count<DisabledPos>(), 1u);

    std::uint32_t seen = 0;
    mRegistry->CreateQuery()->IncludingDisabled().ForEachChunk<DisabledPos>(
        [&](fr::ChunkView view) { seen += static_cast<std::uint32_t>(view.size()); });
    EXPECT_EQ(seen, 2u);

    mRegistry->SetEnabled(b, true);
    mRegistry->ExecuteTasks();
    EXPECT_EQ(mRegistry->CreateQuery()->Count<DisabledPos>(), 2u);
}
