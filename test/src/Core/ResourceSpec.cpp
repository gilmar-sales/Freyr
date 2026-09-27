#include <Freyr/Freyr.hpp>
#include <gtest/gtest.h>

#include "../EmptyApp.hpp"

namespace
{
    struct TimeResource
    {
        float delta = 0.f;
        float total = 0.f;
    };

    struct ScoreResource
    {
        int value = 0;
    };

    class ResourceSpec : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            mApp = skr::ApplicationBuilder()
                       .WithExtension<fr::FreyrExtension>(
                           [](fr::FreyrExtension& freyr)
                           {
                               freyr.WithResource(TimeResource {.delta = 0.016f, .total = 1.f})
                                   .WithOptions([](fr::FreyrOptionsBuilder& options) {
                                       options.WithMaxEntities(64).WithThreadCount(1);
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

TEST_F(ResourceSpec, InsertGetRemoveShouldWork)
{
    ASSERT_TRUE(mRegistry->HasResource<TimeResource>());
    EXPECT_FLOAT_EQ(mRegistry->GetResource<TimeResource>().delta, 0.016f);

    mRegistry->InsertResource(ScoreResource {.value = 42});
    ASSERT_TRUE(mRegistry->HasResource<ScoreResource>());
    EXPECT_EQ(mRegistry->GetResource<ScoreResource>().value, 42);

    EXPECT_TRUE(mRegistry->RemoveResource<ScoreResource>());
    EXPECT_FALSE(mRegistry->HasResource<ScoreResource>());
    EXPECT_FALSE(mRegistry->TryGetResource<ScoreResource>().has_value());
}
