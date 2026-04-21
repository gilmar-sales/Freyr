
#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"

constexpr auto CHUNK_CAPACITY = 128;
constexpr auto CHUNK_COUNT    = 4;
constexpr auto ENTITY_COUNT   = CHUNK_CAPACITY * CHUNK_COUNT;

class ComponentManagerSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto app = skr::ApplicationBuilder().AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
            freyr.WithOptions([](fr::FreyrOptionsBuilder& options) {
                options.WithArchetypeChunkCapacity(CHUNK_CAPACITY);
            });
        });

        mServiceProvider  = app.GetServiceCollection().CreateServiceProvider();
        mComponentManager = mServiceProvider->GetService<fr::ComponentManager>();
        mScene            = mServiceProvider->GetService<fr::Scene>();
    }

    void TearDown() override { mComponentManager.reset(); }

    Ref<fr::ComponentManager> mComponentManager;
    Ref<fr::Scene>            mScene;
    Ref<skr::ServiceProvider> mServiceProvider;
};

TEST_F(ComponentManagerSpec, ComponentManager_ShouldAddSingleComponent)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();

    // Act
    for (auto i = 0; i < ENTITY_COUNT; i++)
        mComponentManager->AddComponent(i, PositionComponent { .x = static_cast<float>(i) });

    mScene->ExecuteTasks();

    // Assert
    for (auto i = 0; i < ENTITY_COUNT; i++)
    {
        const auto has = mComponentManager->HasComponent<PositionComponent>(i);
        ASSERT_TRUE(has);
        ASSERT_EQ(mComponentManager->GetComponent<PositionComponent>(i).x, static_cast<float>(i));
    }
}

TEST_F(ComponentManagerSpec, ComponentManager_ShouldAddMultipleComponents)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<ModelComponent>();

    // Act
    for (unsigned i = 0; i < ENTITY_COUNT; i++)
        mComponentManager->AddComponents<PositionComponent, ModelComponent>(
            i,
            PositionComponent { .x = static_cast<float>(i) },
            ModelComponent { .mesh = i, .material = i, .texture = i });

    mScene->ExecuteTasks();

    // Assert
    for (auto i = 0; i < ENTITY_COUNT; i++)
    {
        const auto has = mComponentManager->HasComponents<PositionComponent, ModelComponent>(i);
        ASSERT_TRUE(has);
        ASSERT_EQ(mComponentManager->GetComponent<PositionComponent>(i).x, static_cast<float>(i));
    }
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldRemoveEntities)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();

    for (auto i = 0; i < ENTITY_COUNT; i++)
        mComponentManager->AddComponent(i, PositionComponent { .x = static_cast<float>(i) });

    mScene->ExecuteTasks();

    // Act
    for (auto i = 0; i < ENTITY_COUNT; i++)
        mComponentManager->EntityDestroyed(i);

    // Assert
    for (auto i = 0; i < ENTITY_COUNT; i++)
        ASSERT_FALSE(mComponentManager->HasComponent<PositionComponent>(i));
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldRemoveComponent)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();

    for (auto i = 0; i < ENTITY_COUNT; i++)
        mComponentManager->AddComponent(i, PositionComponent { .x = static_cast<float>(i) });

    mScene->ExecuteTasks();

    // Act
    for (auto i = 0; i < ENTITY_COUNT; i++)
        mComponentManager->RemoveComponent<PositionComponent>(i);

    mScene->ExecuteTasks();

    // Assert
    for (auto i = 0; i < ENTITY_COUNT; i++)
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
    ASSERT_TRUE(fr::Signature::Make<NameComponent>().Match(archetype->GetSignature()));
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
    for (auto i = 0; i < ENTITY_COUNT; i++)
        mComponentManager->AddComponent(i, PositionComponent { .x = static_cast<float>(i) });
    mComponentManager->AddComponent(0, PositionComponent { .x = 1000.0f });

    mScene->ExecuteTasks();

    // Assert
    for (auto i = 1; i < ENTITY_COUNT; i++)
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
    ASSERT_TRUE(fr::Signature::Make<NameComponent>().Match(archetype->GetSignature()));
    ASSERT_TRUE(fr::Signature::Make<PositionComponent>().Match(archetype->GetSignature()));
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

TEST_F(ComponentManagerSpec, ComponentManagerShouldCreateNewArchetypeWhenNoMatchFound)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();
    mComponentManager->RegisterComponent<ModelComponent>();

    const fr::Entity entity1 = 1;
    const fr::Entity entity2 = 2;

    mComponentManager->AddComponents<PositionComponent, NameComponent>(
        entity1,
        PositionComponent { .x = 100, .y = 200, .z = 300 },
        NameComponent { .name = "First" },
        [](auto, auto&, auto&) {});

    mScene->ExecuteTasks();

    // Act - Add entity with different component combination (ModelComponent)
    // This should create a NEW archetype, not reuse existing
    mComponentManager->AddComponents<PositionComponent, ModelComponent>(
        entity2,
        PositionComponent { .x = 400, .y = 500, .z = 600 },
        ModelComponent { .mesh = 1, .material = 2, .texture = 3 },
        [](auto, auto&, auto&) {});

    mScene->ExecuteTasks();

    const auto [archetype1, _chunk1] = mComponentManager->GetEntityIndex(entity1);
    const auto [archetype2, _chunk2] = mComponentManager->GetEntityIndex(entity2);
    const auto hasBothComponents     = archetype2->HasComponents<PositionComponent, ModelComponent>();

    // Assert - Different archetypes
    ASSERT_NE(archetype1, archetype2);
    ASSERT_TRUE(hasBothComponents);
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldMigrateToExistingArchetypeFromEmpty)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();

    const fr::Entity entity1 = 1;
    const fr::Entity entity2 = 2;
    const fr::Entity entity3 = 3;

    mComponentManager->AddComponents<PositionComponent, NameComponent>(
        entity1,
        PositionComponent { .x = 100, .y = 200, .z = 300 },
        NameComponent { .name = "First" },
        [](auto, auto&, auto&) {});

    mScene->ExecuteTasks();

    const auto [existingArchetype, _] = mComponentManager->GetEntityIndex(entity1);

    // Act - Two more entities join the same archetype
    mComponentManager->AddComponents<PositionComponent, NameComponent>(
        entity2,
        PositionComponent { .x = 400, .y = 500, .z = 600 },
        NameComponent { .name = "Second" },
        [](auto, auto&, auto&) {});

    mComponentManager->AddComponents<PositionComponent, NameComponent>(
        entity3,
        PositionComponent { .x = 700, .y = 800, .z = 900 },
        NameComponent { .name = "Third" },
        [](auto, auto&, auto&) {});

    mScene->ExecuteTasks();

    const auto [archetype2, _chunk2] = mComponentManager->GetEntityIndex(entity2);
    const auto [archetype3, _chunk3] = mComponentManager->GetEntityIndex(entity3);

    // Assert - All entities share the same archetype
    ASSERT_EQ(existingArchetype, archetype2);
    ASSERT_EQ(archetype2, archetype3);
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldHandleThreeComponentsAtOnce)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();
    mComponentManager->RegisterComponent<ModelComponent>();

    constexpr fr::Entity entity = 1;

    // Act - Add 3 components at once
    mComponentManager->AddComponents<PositionComponent, NameComponent, ModelComponent>(
        entity,
        PositionComponent { .x = 100, .y = 200, .z = 300 },
        NameComponent { .name = "Multi-Component Entity" },
        ModelComponent { .mesh = 1, .material = 2, .texture = 3 },
        [](auto, auto&, auto&, auto&) {});

    mScene->ExecuteTasks();

    const auto [archetype, _chunk] = mComponentManager->GetEntityIndex(entity);

    // Assert
    const auto hasPosition = archetype->HasComponent<PositionComponent>();
    const auto hasName     = archetype->HasComponent<NameComponent>();
    const auto hasModel    = archetype->HasComponent<ModelComponent>();

    ASSERT_TRUE(hasPosition);
    ASSERT_TRUE(hasName);
    ASSERT_TRUE(hasModel);
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldAddEntityToExistingArchetypeMultipleTimes)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();

    const fr::Entity entity1 = 1;
    const fr::Entity entity2 = 2;
    const fr::Entity entity3 = 3;
    const fr::Entity entity4 = 4;
    const fr::Entity entity5 = 5;

    mComponentManager->AddComponent(entity1, PositionComponent { .x = 100 });

    mScene->ExecuteTasks();

    const auto [archetype1, _chunk1] = mComponentManager->GetEntityIndex(entity1);

    // Act - Add more entities to same archetype
    mComponentManager->AddComponent(entity2, PositionComponent { .x = 200 });
    mComponentManager->AddComponent(entity3, PositionComponent { .x = 300 });
    mComponentManager->AddComponent(entity4, PositionComponent { .x = 400 });
    mComponentManager->AddComponent(entity5, PositionComponent { .x = 500 });

    mScene->ExecuteTasks();

    const auto [archetype2, _chunk2] = mComponentManager->GetEntityIndex(entity2);
    const auto [archetype3, _chunk3] = mComponentManager->GetEntityIndex(entity3);
    const auto [archetype4, _chunk4] = mComponentManager->GetEntityIndex(entity4);
    const auto [archetype5, _chunk5] = mComponentManager->GetEntityIndex(entity5);

    // Assert - All entities in same archetype
    ASSERT_EQ(archetype1, archetype2);
    ASSERT_EQ(archetype2, archetype3);
    ASSERT_EQ(archetype3, archetype4);
    ASSERT_EQ(archetype4, archetype5);
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldReuseArchetypeForNewEntityAfterMigration)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();
    mComponentManager->RegisterComponent<ModelComponent>();

    const fr::Entity entity1 = 1;
    const fr::Entity entity2 = 2;
    const fr::Entity entity3 = 3;

    mComponentManager->AddComponent(entity1, PositionComponent { .x = 100 });
    mComponentManager->AddComponent(entity1, NameComponent { .name = "First" });

    mComponentManager->AddComponent(entity2, PositionComponent { .x = 200 });
    mComponentManager->AddComponent(entity2, NameComponent { .name = "Second" });

    mScene->ExecuteTasks();

    const auto [archetype1, _] = mComponentManager->GetEntityIndex(entity1);

    // Act - Third entity starts with Position, Name, Model
    mComponentManager->AddComponents<PositionComponent, NameComponent, ModelComponent>(
        entity3,
        PositionComponent { .x = 300 },
        NameComponent { .name = "Third" },
        ModelComponent { .mesh = 1 },
        [](auto, auto&, auto&, auto&) {});

    mScene->ExecuteTasks();

    const auto [archetype3, _chunk3] = mComponentManager->GetEntityIndex(entity3);
    const auto hasBothComponents     = archetype3->HasComponents<PositionComponent, NameComponent, ModelComponent>();

    // Assert - Different archetype
    ASSERT_NE(archetype1, archetype3);
    ASSERT_TRUE(hasBothComponents);

    // Act - Add another entity with the same 3 components
    constexpr fr::Entity entity4 = 4;
    mComponentManager->AddComponents<PositionComponent, NameComponent, ModelComponent>(
        entity4,
        PositionComponent { .x = 400 },
        NameComponent { .name = "Fourth" },
        ModelComponent { .mesh = 2 },
        [](auto, auto&, auto&, auto&) {});

    mScene->ExecuteTasks();

    const auto [archetype4, _chunk4] = mComponentManager->GetEntityIndex(entity4);

    // Assert - Should reuse the same archetype
    ASSERT_EQ(archetype3, archetype4);
}