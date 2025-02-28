#include "gtest/gtest.h"

#include "../Components/PositionComponent.hpp"
#include <Freyr/Freyr.hpp>

class ComponentManagerSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        const auto serviceCollection = std::make_shared<ServiceCollection>();
        const auto provider = serviceCollection->CreateServiceProvider();

        fr::SceneBuilder(serviceCollection)
            .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                builder.SetArchetypeChunkCapacity(1024);
            })
            .Build(*provider);

        mComponentManager = provider->GetService<fr::ComponentManager>();
    }

    void TearDown() override { mComponentManager.reset(); }

    std::shared_ptr<fr::ComponentManager> mComponentManager;
};

TEST_F(ComponentManagerSpec, ComponentManagerShouldAddEntities)
{
    mComponentManager->RegisterComponent<PositionComponent>();

    for (auto i = 0; i < 1200; i++)
        mComponentManager->AddComponent(0, PositionComponent {});

    ASSERT_EQ(mComponentManager->GetComponent<PositionComponent>(0).x, 0.0f);
}