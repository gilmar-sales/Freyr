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
