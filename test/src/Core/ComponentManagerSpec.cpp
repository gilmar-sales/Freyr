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

    mScene->ExecuteTasks();

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
    const auto hasComponent = mComponentManager->HasComponent<PositionComponent>(2);

    // Assert
    ASSERT_FALSE(hasComponent);
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldAddMultipleComponentsSeparately)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();

    // Act
    mComponentManager->AddComponents<NameComponent>(2, NameComponent { .name = "New Entity" }, [](auto, const auto&) {
    });
    mComponentManager->AddComponents<PositionComponent>(
        2, PositionComponent { .x = 1000, .y = 5000, .z = 2000 }, [](auto, auto) {});

    mScene->ExecuteTasks();

    const auto hasComponent                = mComponentManager->HasComponent<NameComponent>(2);
    const auto [archetype, archetypeChunk] = mComponentManager->GetEntityIndex(2);

    // Assert
    ASSERT_TRUE(hasComponent);
    ASSERT_TRUE(archetype->HasComponent<NameComponent>());
    ASSERT_TRUE(fr::MakeSignature<NameComponent>().Match(archetype->GetSignature()));
    mComponentManager->TryGetComponents<NameComponent, PositionComponent>(
        2, [](const NameComponent& name, const PositionComponent& position) {
            ASSERT_STREQ("New Entity", name.name.c_str());
            ASSERT_EQ(position.x, 1000);
            ASSERT_EQ(position.y, 5000);
            ASSERT_EQ(position.z, 2000);
        });
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldReplaceComponentWhenAddComponent)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();

    // Act
    for (auto i = 0; i < 1200; i++)
        mComponentManager->AddComponent(i, PositionComponent { .x = static_cast<float>(i) });
    mComponentManager->AddComponent(0, PositionComponent { .x = 1000.0f });

    mScene->ExecuteTasks();

    // Assert
    for (auto i = 1; i < 1200; i++)
        ASSERT_EQ(mComponentManager->GetComponent<PositionComponent>(i).x, static_cast<float>(i));
    ASSERT_EQ(mComponentManager->GetComponent<PositionComponent>(0).x, 1000.0f);
}