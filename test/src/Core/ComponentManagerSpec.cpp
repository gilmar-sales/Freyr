#include "../Components/NameComponent.hpp"

#include "gtest/gtest.h"

#include "../Components/PositionComponent.hpp"
#include <Freyr/Freyr.hpp>

class ComponentManagerSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto app = skr::ApplicationBuilder().AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
            freyr.WithOptions([](fr::FreyrOptionsBuilder& builder) { builder.WithArchetypeChunkCapacity(1024); });
        });

        mServiceProvider = app.GetServiceCollection().CreateServiceProvider();

        mComponentManager = mServiceProvider->GetService<fr::ComponentManager>();
        mScene            = mServiceProvider->GetService<fr::Scene>();
    }

    void TearDown() override { mComponentManager.reset(); }

    Ref<fr::ComponentManager> mComponentManager;
    Ref<fr::Scene>            mScene;
    Ref<skr::ServiceProvider> mServiceProvider;
};

TEST_F(ComponentManagerSpec, ComponentManagerShouldAddEntities)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();

    // Act
    for (auto i = 0; i < 1200; i++)
        mComponentManager->AddComponent(i, PositionComponent { .x = (float) i });

    // Assert
    for (auto i = 0; i < 1200; i++)
        ASSERT_EQ(mComponentManager->GetComponent<PositionComponent>(i).x, (float) i);
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldRemoveEntities)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();

    for (auto i = 0; i < 1200; i++)
        mComponentManager->AddComponent(i, PositionComponent { .x = (float) i });

    // Act
    for (auto i = 0; i < 1200; i++)
        mComponentManager->EntityDestroyed(i);

    // Assert
    for (auto i = 0; i < 1200; i++)
        ASSERT_FALSE(mComponentManager->HasComponent<PositionComponent>(i));
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldReturnFalseToHasComponentForAnEmptyEntity)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();

    // Act
    auto hasComponent = mComponentManager->HasComponent<PositionComponent>(2);

    // Assert
    ASSERT_FALSE(hasComponent);
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldAddMultipleComponentsSeparately)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();

    // Act
    mComponentManager->AddComponents<NameComponent>(2, NameComponent { .name = "New Entity" }, [](auto, auto) {});
    mScene->ExecuteTasks();
    mComponentManager->AddComponents<PositionComponent>(
        2, PositionComponent { .x = 1000, .y = 5000, .z = 2000 }, [](auto, auto) {});
    mScene->ExecuteTasks();

    auto hasComponent = mComponentManager->HasComponent<NameComponent>(2);
    auto entityIndex  = mComponentManager->GetEntityIndex(2);

    // Assert
    ASSERT_TRUE(hasComponent);
    ASSERT_TRUE(entityIndex.archetype->HasComponent<NameComponent>());
    ASSERT_TRUE(fr::MakeSignature<NameComponent>().Match(entityIndex.archetype->GetSignature()));
    mComponentManager->TryGetComponents<NameComponent, PositionComponent>(
        2, [](NameComponent& name, PositionComponent& position) {
            ASSERT_STREQ("New Entity", name.name.c_str());
            ASSERT_EQ(position.x, 1000);
            ASSERT_EQ(position.y, 5000);
            ASSERT_EQ(position.z, 2000);
        });
}