
#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"

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
        2,
        PositionComponent { .x = 1000, .y = 5000, .z = 2000 },
        [](auto, auto) {});

    mScene->ExecuteTasks();

    const auto hasComponent                = mComponentManager->HasComponent<NameComponent>(2);
    const auto [archetype, archetypeChunk] = mComponentManager->GetEntityIndex(2);

    // Assert
    ASSERT_TRUE(hasComponent);
    ASSERT_TRUE(archetype->HasComponent<NameComponent>());
    ASSERT_TRUE(fr::MakeSignature<NameComponent>().Match(archetype->GetSignature()));
    mComponentManager->TryGetComponents<NameComponent, PositionComponent>(
        2,
        [](const NameComponent& name, const PositionComponent& position) {
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

TEST_F(ComponentManagerSpec, ComponentManagerShouldMigrateEntityToExistingArchetype)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();

    mComponentManager->AddComponents<NameComponent>(1, NameComponent { .name = "First Entity" }, [](auto, const auto&) {
    });

    mComponentManager->AddComponents<PositionComponent, NameComponent>(
        2,
        PositionComponent { .x = 1000, .y = 5000, .z = 2000 },
        NameComponent { .name = "Second Entity" },
        [](auto, auto&, auto&) {});

    // Act
    mComponentManager->AddComponents<PositionComponent>(
        1,
        PositionComponent { .x = 2000, .y = 3000, .z = 5000 },
        [](auto, const auto&) {});

    mScene->ExecuteTasks();

    const auto hasComponents               = mComponentManager->HasComponents<NameComponent, PositionComponent>(1);
    const auto [archetype, archetypeChunk] = mComponentManager->GetEntityIndex(1);
    const auto archetypeHasComponents      = archetype->HasComponents<NameComponent, PositionComponent>();

    // Assert
    ASSERT_TRUE(hasComponents);
    ASSERT_TRUE(archetypeHasComponents);
    ASSERT_TRUE(fr::MakeSignature<NameComponent>().Match(archetype->GetSignature()));
    ASSERT_TRUE(fr::MakeSignature<PositionComponent>().Match(archetype->GetSignature()));
    mComponentManager->TryGetComponents<NameComponent, PositionComponent>(
        1,
        [](const NameComponent& name, const PositionComponent& position) {
            ASSERT_STREQ("First Entity", name.name.c_str());
            ASSERT_EQ(position.x, 2000);
            ASSERT_EQ(position.y, 3000);
            ASSERT_EQ(position.z, 5000);
        });
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldReturnFalseWhenEntityDoesNotExists)
{
    // Arrange

    // Act
    const auto hasSingle   = mComponentManager->HasComponent<NameComponent>(1);
    const auto hasMultiple = mComponentManager->HasComponents<NameComponent, PositionComponent>(1);
    const auto couldGetInexistentComponent =
        mComponentManager->TryGetComponents<NameComponent, PositionComponent>(1, [](auto&, auto&) {});

    // Assert
    ASSERT_FALSE(hasSingle);
    ASSERT_FALSE(hasMultiple);
    ASSERT_FALSE(couldGetInexistentComponent);
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldReturnFalseWhenEntityDoesNotHaveTheComponents)
{
    // Arrange
    mComponentManager->AddComponent(1, NameComponent { .name = "First Entity" });
    mScene->ExecuteTasks();

    // Act
    const auto hasSingle   = mComponentManager->HasComponent<NameComponent>(1);
    const auto hasMultiple = mComponentManager->HasComponents<NameComponent, PositionComponent>(1);
    const auto couldGetInexistentComponent =
        mComponentManager->TryGetComponents<NameComponent, PositionComponent>(1, [](auto&, auto&) {});

    // Assert
    ASSERT_TRUE(hasSingle);
    ASSERT_FALSE(hasMultiple);
    ASSERT_FALSE(couldGetInexistentComponent);
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldNotMigrateWhenAddingSameComponent)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();

    const fr::Entity entity = 1;
    mComponentManager->AddComponents<PositionComponent>(
        entity,
        PositionComponent { .x = 100, .y = 200, .z = 300 },
        [](auto, const auto&) {});

    mScene->ExecuteTasks();

    const auto [archetypeBefore, chunkBefore] = mComponentManager->GetEntityIndex(entity);

    // Act - Add same component type again (should NOT trigger migration)
    mComponentManager->AddComponents<PositionComponent>(
        entity,
        PositionComponent { .x = 400, .y = 500, .z = 600 },
        [](auto, const auto&) {});

    mScene->ExecuteTasks();

    const auto [archetypeAfter, chunkAfter] = mComponentManager->GetEntityIndex(entity);

    // Assert - Entity should stay in the same archetype (no migration)
    ASSERT_EQ(archetypeBefore, archetypeAfter);
    ASSERT_EQ(chunkBefore, chunkAfter);
    ASSERT_EQ(mComponentManager->GetComponent<PositionComponent>(entity).x, 400.0f);
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldMigrateEntityToNewArchetype)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();

    const fr::Entity entity = 1;
    mComponentManager->AddComponents<PositionComponent>(
        entity,
        PositionComponent { .x = 100, .y = 200, .z = 300 },
        [](auto, const auto&) {});

    mScene->ExecuteTasks();

    const auto [archetypeBefore, _] = mComponentManager->GetEntityIndex(entity);

    // Act - Add NameComponent to trigger migration to NEW archetype
    mComponentManager->AddComponents<NameComponent>(
        entity,
        NameComponent { .name = "Migrated Entity" },
        [](auto, const auto&) {});

    mScene->ExecuteTasks();

    const auto [archetypeAfter, chunkAfter] = mComponentManager->GetEntityIndex(entity);
    const auto  hasBothComponents           = archetypeAfter->HasComponents<PositionComponent, NameComponent>();
    const auto& nameComponent               = mComponentManager->GetComponent<NameComponent>(entity);

    // Assert - Entity should have migrated to a new archetype with both components
    ASSERT_NE(archetypeBefore, archetypeAfter);
    ASSERT_TRUE(hasBothComponents);
    ASSERT_STREQ(nameComponent.name.c_str(), "Migrated Entity");
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldReuseExistingArchetypeForNewEntity)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();

    const fr::Entity entity1 = 1;
    const fr::Entity entity2 = 2;

    mComponentManager->AddComponents<PositionComponent, NameComponent>(
        entity1,
        PositionComponent { .x = 100, .y = 200, .z = 300 },
        NameComponent { .name = "First Entity" },
        [](auto, auto&, auto&) {});

    mScene->ExecuteTasks();

    const auto [archetype1, _] = mComponentManager->GetEntityIndex(entity1);

    // Act - Add entity with same component types - should reuse existing archetype
    mComponentManager->AddComponents<PositionComponent, NameComponent>(
        entity2,
        PositionComponent { .x = 400, .y = 500, .z = 600 },
        NameComponent { .name = "Second Entity" },
        [](auto, auto&, auto&) {});

    mScene->ExecuteTasks();

    const auto [archetype2, _chunk] = mComponentManager->GetEntityIndex(entity2);
    const auto hasBothComponents    = archetype1->HasComponents<PositionComponent, NameComponent>();

    // Assert - Both entities should be in the SAME archetype
    ASSERT_EQ(archetype1, archetype2);
    ASSERT_TRUE(hasBothComponents);
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldMigrateToExistingArchetypeDuringUpdate)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();

    const fr::Entity entity1 = 1;
    const fr::Entity entity2 = 2;

    mComponentManager->AddComponents<PositionComponent>(
        entity1,
        PositionComponent { .x = 100, .y = 200, .z = 300 },
        [](auto, const auto&) {});

    mComponentManager->AddComponents<PositionComponent, NameComponent>(
        entity2,
        PositionComponent { .x = 400, .y = 500, .z = 600 },
        NameComponent { .name = "Target Entity" },
        [](auto, auto&, auto&) {});

    mScene->ExecuteTasks();

    const auto [targetArchetype, _] = mComponentManager->GetEntityIndex(entity2);

    // Act - Add NameComponent to entity1, should find existing archetype with (Position, Name)
    mComponentManager->AddComponents<NameComponent>(
        entity1,
        NameComponent { .name = "Migrated" },
        [](auto, const auto&) {});

    mScene->ExecuteTasks();

    const auto [migratedArchetype, _chunk] = mComponentManager->GetEntityIndex(entity1);
    const auto hasBothComponents           = migratedArchetype->HasComponents<PositionComponent, NameComponent>();

    // Assert - Entity1 should migrate to the EXISTING archetype that has (Position, Name)
    ASSERT_EQ(targetArchetype, migratedArchetype);
    ASSERT_TRUE(hasBothComponents);
}