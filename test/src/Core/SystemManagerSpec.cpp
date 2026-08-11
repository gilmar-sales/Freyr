#include <gtest/gtest.h>

#include <Freyr/Freyr.hpp>

#include "../Components/PositionComponent.hpp"
#include "../EmptyApp.hpp"
#include "../Systems/CounterSystem.hpp"
#include "../Systems/MovementSystem.hpp"

class SystemManagerSpec : public ::testing::Test
{
};

TEST_F(SystemManagerSpec, GetSystemLabelReturnsRegisteredSystemTypeName)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Main").WithSystem<CounterSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    auto systemManager = app->GetRootServiceProvider()->GetService<fr::SystemManager>();

    EXPECT_EQ(systemManager->GetSystemLabel(fr::GetSystemId<CounterSystem>()),
              refl::type_name<CounterSystem>());
}

TEST_F(SystemManagerSpec, GetSystemLabelMapsEachSystemIdIndependently)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Main")
                                   .WithSystem<CounterSystem>()
                                   .WithSystem<MovementSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    auto systemManager = app->GetRootServiceProvider()->GetService<fr::SystemManager>();

    EXPECT_EQ(systemManager->GetSystemLabel(fr::GetSystemId<CounterSystem>()),
              refl::type_name<CounterSystem>());
    EXPECT_EQ(systemManager->GetSystemLabel(fr::GetSystemId<MovementSystem>()),
              refl::type_name<MovementSystem>());
    EXPECT_NE(systemManager->GetSystemLabel(fr::GetSystemId<CounterSystem>()),
              systemManager->GetSystemLabel(fr::GetSystemId<MovementSystem>()));
}

TEST_F(SystemManagerSpec, FindPipelineIdReturnsRegisteredPipelineByName)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Main").WithSystem<MovementSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    const auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();

    const auto mainId = registry->FindPipelineId("Main");
    ASSERT_TRUE(mainId.has_value());
    EXPECT_EQ(*mainId, 0);
    EXPECT_FALSE(registry->FindPipelineId("Missing").has_value());
}

TEST_F(SystemManagerSpec, RuntimeRegisterSystemIsInvokedOnUpdate)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Main").WithSystem<MovementSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    const auto provider = app->GetRootServiceProvider();
    const auto registry = provider->GetService<fr::Registry>();

    const auto pipelineId = registry->FindPipelineId("Main");
    ASSERT_TRUE(pipelineId.has_value());

    registry->RegisterSystem<CounterSystem>(*pipelineId);
    ASSERT_TRUE(registry->IsSystemRegistered<CounterSystem>());

    registry->Update(0.016f);

    const auto counter = provider->GetService<CounterSystem>();
    EXPECT_EQ(counter->UpdateCount, 1);
}

TEST_F(SystemManagerSpec, RuntimeUnregisterSystemStopsBeingInvoked)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Main").WithSystem<CounterSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    const auto provider = app->GetRootServiceProvider();
    const auto registry = provider->GetService<fr::Registry>();
    const auto counter  = provider->GetService<CounterSystem>();

    registry->Update(0.016f);
    ASSERT_EQ(counter->UpdateCount, 1);

    EXPECT_TRUE(registry->UnregisterSystem<CounterSystem>());
    EXPECT_FALSE(registry->IsSystemRegistered<CounterSystem>());
    EXPECT_FALSE(registry->UnregisterSystem<CounterSystem>());

    registry->Update(0.016f);
    EXPECT_EQ(counter->UpdateCount, 1);
}

TEST_F(SystemManagerSpec, RuntimeReloadSystemUnregisterThenRegisterAgain)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Main").WithSystem<MovementSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    const auto provider   = app->GetRootServiceProvider();
    const auto registry   = provider->GetService<fr::Registry>();
    const auto pipelineId = *registry->FindPipelineId("Main");

    registry->RegisterSystem<CounterSystem>(pipelineId);
    registry->Update(0.016f);
    EXPECT_EQ(provider->GetService<CounterSystem>()->UpdateCount, 1);

    ASSERT_TRUE(registry->UnregisterSystem<CounterSystem>());

    registry->RegisterSystem<CounterSystem>(pipelineId);
    registry->Update(0.016f);
    EXPECT_EQ(provider->GetService<CounterSystem>()->UpdateCount, 1);
}

TEST_F(SystemManagerSpec, RuntimeRegisterSystemAppendsAfterStartupSystems)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Main").WithSystem<MovementSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    const auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();
    const auto systemManager = app->GetRootServiceProvider()->GetService<fr::SystemManager>();

    registry->RegisterSystem<CounterSystem>(*registry->FindPipelineId("Main"));

    EXPECT_TRUE(systemManager->IsSystemRegistered<MovementSystem>());
    EXPECT_TRUE(systemManager->IsSystemRegistered<CounterSystem>());
    EXPECT_EQ(systemManager->GetSystemLabel(fr::GetSystemId<MovementSystem>()),
              refl::type_name<MovementSystem>());
    EXPECT_EQ(systemManager->GetSystemLabel(fr::GetSystemId<CounterSystem>()),
              refl::type_name<CounterSystem>());
}
