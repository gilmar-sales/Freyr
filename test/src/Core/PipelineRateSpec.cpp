#include <gtest/gtest.h>

#include <Freyr/Freyr.hpp>
#include <Freyr/Core/Pipeline.hpp>

#include "../Components/PositionComponent.hpp"
#include "../EmptyApp.hpp"
#include "../Systems/CounterSystem.hpp"

#include <vector>

struct PipelineRateSpec : public ::testing::Test
{
};

TEST_F(PipelineRateSpec, PipelineNameSurvivesSourceStringDestruction)
{
    const fr::Pipeline pipeline = [] {
        const std::string temporaryName = "HeapPoisonTarget";
        return fr::Pipeline(temporaryName, 0.f);
    }();

    std::vector<std::string> poison;
    poison.reserve(4096);
    for (int i = 0; i < 4096; ++i)
    {
        poison.emplace_back(64, static_cast<char>('A' + (i % 26)));
    }

    EXPECT_EQ(pipeline.Name, "HeapPoisonTarget");
}

TEST_F(PipelineRateSpec, PipelineWithRateZeroRunsEveryUpdate)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("EveryFrame").WithRate(0.f).WithSystem<CounterSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();
    auto counter  = app->GetRootServiceProvider()->GetService<CounterSystem>();

    registry->Update(0.016f);
    registry->Update(0.016f);
    registry->Update(0.016f);

    EXPECT_EQ(counter->UpdateCount, 3);
    EXPECT_FLOAT_EQ(counter->LastDeltaTime, 0.016f);
}

TEST_F(PipelineRateSpec, PipelineAccumulatorDoesNotFireUntilIntervalElapsed)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("TenHz").WithRate(10.f).WithSystem<CounterSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();
    auto counter  = app->GetRootServiceProvider()->GetService<CounterSystem>();

    registry->Update(0.05f);
    EXPECT_EQ(counter->UpdateCount, 0);

    registry->Update(0.05f);
    EXPECT_EQ(counter->UpdateCount, 1);
    EXPECT_FLOAT_EQ(counter->LastDeltaTime, 0.1f);
}

TEST_F(PipelineRateSpec, PipelineWithoutWithRateDefaultsToEveryFrame)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("DefaultRate").WithSystem<CounterSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();
    auto counter  = app->GetRootServiceProvider()->GetService<CounterSystem>();

    registry->Update(1.f);
    registry->Update(1.f);

    EXPECT_EQ(counter->UpdateCount, 2);
    EXPECT_FLOAT_EQ(counter->LastDeltaTime, 1.f);
}
