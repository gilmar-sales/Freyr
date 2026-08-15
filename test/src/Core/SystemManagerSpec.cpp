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

TEST_F(SystemManagerSpec, DisabledPipelineSkipsSystemUpdatesUntilReenabled)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>()
                           .WithPipeline([](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Presentation").WithSystem<MovementSystem>();
                           })
                           .WithPipeline([](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Simulation").WithSystem<CounterSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    const auto provider      = app->GetRootServiceProvider();
    const auto registry      = provider->GetService<fr::Registry>();
    const auto systemManager = provider->GetService<fr::SystemManager>();
    const auto simId         = registry->FindPipelineId("Simulation");
    ASSERT_TRUE(simId.has_value());

    EXPECT_TRUE(registry->IsPipelineEnabled(*simId));
    registry->SetPipelineEnabled(*simId, false);
    EXPECT_FALSE(registry->IsPipelineEnabled(*simId));
    EXPECT_FALSE(systemManager->IsPipelineEnabled(*simId));

    registry->Update(0.016f);
    const auto counter = provider->GetService<CounterSystem>();
    EXPECT_EQ(counter->UpdateCount, 0);

    registry->SetPipelineEnabled(*simId, true);
    registry->Update(0.016f);
    EXPECT_EQ(counter->UpdateCount, 1);
}

TEST_F(SystemManagerSpec, ForEachPipelineExposesRegisteredSystemsInOrder)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Main")
                                   .WithSystem<MovementSystem>()
                                   .WithSystem<CounterSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    const auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();

    EXPECT_EQ(registry->GetPipelineCount(), 1);
    const auto pipeline = registry->GetPipeline(0);
    EXPECT_EQ(pipeline.Name, "Main");
    ASSERT_EQ(pipeline.Systems.size(), 2u);
    EXPECT_EQ(pipeline.Systems[0], fr::GetSystemId<MovementSystem>());
    EXPECT_EQ(pipeline.Systems[1], fr::GetSystemId<CounterSystem>());
    EXPECT_EQ(registry->FindPipelineContaining(fr::GetSystemId<CounterSystem>()), 0);
}

TEST_F(SystemManagerSpec, RegisterSystemAtIndexInsertsBeforeExistingSystems)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Main").WithSystem<MovementSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    const auto registry   = app->GetRootServiceProvider()->GetService<fr::Registry>();
    const auto pipelineId = *registry->FindPipelineId("Main");

    registry->RegisterSystem<CounterSystem>(pipelineId, 0);

    const auto systems = registry->GetPipeline(pipelineId).Systems;
    ASSERT_EQ(systems.size(), 2u);
    EXPECT_EQ(systems[0], fr::GetSystemId<CounterSystem>());
    EXPECT_EQ(systems[1], fr::GetSystemId<MovementSystem>());
}

TEST_F(SystemManagerSpec, MoveSystemReordersWithinPipeline)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Main")
                                   .WithSystem<MovementSystem>()
                                   .WithSystem<CounterSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    const auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();

    EXPECT_TRUE(registry->MoveSystem(fr::GetSystemId<CounterSystem>(), 0, 0));

    const auto systems = registry->GetPipeline(0).Systems;
    ASSERT_EQ(systems.size(), 2u);
    EXPECT_EQ(systems[0], fr::GetSystemId<CounterSystem>());
    EXPECT_EQ(systems[1], fr::GetSystemId<MovementSystem>());
}

TEST_F(SystemManagerSpec, MoveSystemTransfersBetweenPipelines)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>()
                           .WithPipeline([](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Presentation").WithSystem<MovementSystem>();
                           })
                           .WithPipeline([](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Simulation").WithSystem<CounterSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    const auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();
    const auto simId    = *registry->FindPipelineId("Simulation");
    const auto presId   = *registry->FindPipelineId("Presentation");

    EXPECT_TRUE(registry->MoveSystem(fr::GetSystemId<MovementSystem>(), simId, 0));

    EXPECT_TRUE(registry->GetPipeline(presId).Systems.empty());
    const auto simSystems = registry->GetPipeline(simId).Systems;
    ASSERT_EQ(simSystems.size(), 2u);
    EXPECT_EQ(simSystems[0], fr::GetSystemId<MovementSystem>());
    EXPECT_EQ(simSystems[1], fr::GetSystemId<CounterSystem>());
    EXPECT_EQ(registry->FindPipelineContaining(fr::GetSystemId<MovementSystem>()), simId);
}

TEST_F(SystemManagerSpec, SetPipelineNameAndRateAreReadable)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Main").WithSystem<CounterSystem>();
                           });
                   })
                   .Build<EmptyApp>();

    const auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();

    registry->SetPipelineName(0, "Simulation");
    registry->SetPipelineRate(0, 10.f);

    EXPECT_EQ(registry->FindPipelineId("Simulation"), 0);
    EXPECT_FLOAT_EQ(registry->GetPipeline(0).Rate, 0.1f);
}

TEST_F(SystemManagerSpec, MoveSystemReturnsFalseWhenSystemIsNotRegistered)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>().WithPipeline(
                           [](fr::PipelineBuilder& pipeline) { pipeline.WithName("Main"); });
                   })
                   .Build<EmptyApp>();

    const auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();

    EXPECT_FALSE(registry->MoveSystem(fr::GetSystemId<CounterSystem>(), 0, 0));
}
