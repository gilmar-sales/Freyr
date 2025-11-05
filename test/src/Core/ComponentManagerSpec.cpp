#include "gtest/gtest.h"

#include "../Components/PositionComponent.hpp"
#include <Freyr/Freyr.hpp>

class ComponentManagerSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto app = skr::ApplicationBuilder().AddExtension<fr::FreyrExtension>(
            [](fr::FreyrExtension& freyr) {
                freyr.WithOptions([](fr::FreyrOptionsBuilder& builder) {
                    builder.SetArchetypeChunkCapacity(1024);
                });
            });

        const auto provider =
            app.GetServiceCollection().CreateServiceProvider();

        mComponentManager = provider->GetService<fr::ComponentManager>();
    }

    void TearDown() override { mComponentManager.reset(); }

    Ref<fr::ComponentManager> mComponentManager;
};

TEST_F(ComponentManagerSpec, ComponentManagerShouldAddEntities)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();

    // Act
    for (auto i = 0; i < 1200; i++)
        mComponentManager->AddComponent(i,
                                        PositionComponent { .x = (float) i });

    // Assert
    for (auto i = 0; i < 1200; i++)
        ASSERT_EQ(mComponentManager->GetComponent<PositionComponent>(i).x,
                  (float) i);
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldRemoveEntities)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();

    for (auto i = 0; i < 5; i++)
        mComponentManager->AddComponent(i,
                                        PositionComponent { .x = (float) i });

    // Act
    for (auto i = 0; i < 5; i++)
        mComponentManager->EntityDestroyed(i);

    for (auto i = 0; i < 5; i++)
        ASSERT_DEATH(mComponentManager->GetComponent<PositionComponent>(i).x, ".*");
}