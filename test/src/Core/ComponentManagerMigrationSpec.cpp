#include "ComponentManagerTestSupport.hpp"

#include "../Components/DecayComponent.hpp"
#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "../Components/VelocityComponent.hpp"

TEST_F(ComponentManagerSpec, ComponentManagerShouldMigrateEntityToExistingArchetype)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();

    mComponentManager->AddComponents<NameComponent>(
        1,
        NameComponent { .name = "First Entity" },
        [](auto, const auto&) {});

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

    mRegistry->ExecuteTasks();

    const auto hasComponents =
        mComponentManager->HasComponents<NameComponent, PositionComponent>(1);
    const auto [archetype, archetypeChunk] = mComponentManager->GetEntityIndex(1);
    const auto archetypeHasComponents =
        archetype->HasComponents<NameComponent, PositionComponent>();

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

    mRegistry->ExecuteTasks();

    const auto [archetypeBefore, chunkBefore] = mComponentManager->GetEntityIndex(entity);

    // Act - Add same component type again (should NOT trigger migration)
    mComponentManager->AddComponents<PositionComponent>(
        entity,
        PositionComponent { .x = 400, .y = 500, .z = 600 },
        [](auto, const auto&) {});

    mRegistry->ExecuteTasks();

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

    mRegistry->ExecuteTasks();

    const auto [archetypeBefore, _] = mComponentManager->GetEntityIndex(entity);

    // Act - Add NameComponent to trigger migration to NEW archetype
    mComponentManager->AddComponents<NameComponent>(
        entity,
        NameComponent { .name = "Migrated Entity" },
        [](auto, const auto&) {});

    mRegistry->ExecuteTasks();

    const auto [archetypeAfter, chunkAfter] = mComponentManager->GetEntityIndex(entity);
    const auto hasBothComponents =
        archetypeAfter->HasComponents<PositionComponent, NameComponent>();
    const auto& nameComponent = mComponentManager->GetComponent<NameComponent>(entity);

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

    mRegistry->ExecuteTasks();

    const auto [archetype1, _] = mComponentManager->GetEntityIndex(entity1);

    // Act - Add entity with same component types - should reuse existing archetype
    mComponentManager->AddComponents<PositionComponent, NameComponent>(
        entity2,
        PositionComponent { .x = 400, .y = 500, .z = 600 },
        NameComponent { .name = "Second Entity" },
        [](auto, auto&, auto&) {});

    mRegistry->ExecuteTasks();

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

    mRegistry->ExecuteTasks();

    const auto [targetArchetype, _] = mComponentManager->GetEntityIndex(entity2);

    // Act - Add NameComponent to entity1, should find existing archetype with (Position, Name)
    mComponentManager->AddComponents<NameComponent>(
        entity1,
        NameComponent { .name = "Migrated" },
        [](auto, const auto&) {});

    mRegistry->ExecuteTasks();

    const auto [migratedArchetype, _chunk] = mComponentManager->GetEntityIndex(entity1);
    const auto hasBothComponents =
        migratedArchetype->HasComponents<PositionComponent, NameComponent>();

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

    mRegistry->ExecuteTasks();

    // Act - Add entity with different component combination (ModelComponent)
    // This should create a NEW archetype, not reuse existing
    mComponentManager->AddComponents<PositionComponent, ModelComponent>(
        entity2,
        PositionComponent { .x = 400, .y = 500, .z = 600 },
        ModelComponent { .mesh = 1, .material = 2, .texture = 3 },
        [](auto, auto&, auto&) {});

    mRegistry->ExecuteTasks();

    const auto [archetype1, _chunk1] = mComponentManager->GetEntityIndex(entity1);
    const auto [archetype2, _chunk2] = mComponentManager->GetEntityIndex(entity2);
    const auto hasBothComponents = archetype2->HasComponents<PositionComponent, ModelComponent>();

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

    mRegistry->ExecuteTasks();

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

    mRegistry->ExecuteTasks();

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

    mRegistry->ExecuteTasks();

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

    mRegistry->ExecuteTasks();

    const auto [archetype1, _chunk1] = mComponentManager->GetEntityIndex(entity1);

    // Act - Add more entities to same archetype
    mComponentManager->AddComponent(entity2, PositionComponent { .x = 200 });
    mComponentManager->AddComponent(entity3, PositionComponent { .x = 300 });
    mComponentManager->AddComponent(entity4, PositionComponent { .x = 400 });
    mComponentManager->AddComponent(entity5, PositionComponent { .x = 500 });

    mRegistry->ExecuteTasks();

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

    mRegistry->ExecuteTasks();

    const auto [archetype1, _] = mComponentManager->GetEntityIndex(entity1);

    // Act - Third entity starts with Position, Name, Model
    mComponentManager->AddComponents<PositionComponent, NameComponent, ModelComponent>(
        entity3,
        PositionComponent { .x = 300 },
        NameComponent { .name = "Third" },
        ModelComponent { .mesh = 1 },
        [](auto, auto&, auto&, auto&) {});

    mRegistry->ExecuteTasks();

    const auto [archetype3, _chunk3] = mComponentManager->GetEntityIndex(entity3);
    const auto hasBothComponents =
        archetype3->HasComponents<PositionComponent, NameComponent, ModelComponent>();

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

    mRegistry->ExecuteTasks();

    const auto [archetype4, _chunk4] = mComponentManager->GetEntityIndex(entity4);

    // Assert - Should reuse the same archetype
    ASSERT_EQ(archetype3, archetype4);
}

TEST_F(ComponentManagerSpec, MigrationShouldPreservePendingComponentWrites)
{
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();

    constexpr fr::Entity entity = 1;

    mComponentManager->AddComponents<PositionComponent>(
        entity,
        PositionComponent { .x = 42.f, .y = 7.f, .z = 3.f },
        [](auto, const auto&) {});

    mComponentManager->AddComponents<NameComponent>(
        entity,
        NameComponent { .name = "AfterMigration" },
        [](auto, const auto&) {});

    mRegistry->ExecuteTasks();

    ASSERT_TRUE((mComponentManager->HasComponents<PositionComponent, NameComponent>(entity)));
    ASSERT_FLOAT_EQ(mComponentManager->GetComponent<PositionComponent>(entity).x, 42.f);
    ASSERT_FLOAT_EQ(mComponentManager->GetComponent<PositionComponent>(entity).y, 7.f);
    ASSERT_FLOAT_EQ(mComponentManager->GetComponent<PositionComponent>(entity).z, 3.f);
    ASSERT_STREQ(mComponentManager->GetComponent<NameComponent>(entity).name.c_str(),
                 "AfterMigration");
}

TEST_F(ComponentManagerSpec, RemovePartialComponentShouldMigrateToExistingArchetype)
{
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();
    mComponentManager->RegisterComponent<ModelComponent>();

    constexpr fr::Entity keepEntity    = 1;
    constexpr fr::Entity migrateEntity = 2;

    mComponentManager->AddComponents<PositionComponent, NameComponent>(
        keepEntity,
        PositionComponent { .x = 1.f },
        NameComponent { .name = "keep" },
        [](auto, auto&, auto&) {});

    mComponentManager->AddComponents<PositionComponent, NameComponent, ModelComponent>(
        migrateEntity,
        PositionComponent { .x = 2.f, .y = 3.f, .z = 4.f },
        NameComponent { .name = "migrate" },
        ModelComponent { .mesh = 9 },
        [](auto, auto&, auto&, auto&) {});

    mRegistry->ExecuteTasks();

    const auto [targetArchetype, _] = mComponentManager->GetEntityIndex(keepEntity);

    mComponentManager->RemoveComponent<ModelComponent>(migrateEntity);
    mRegistry->ExecuteTasks();

    const auto [migratedArchetype, _chunk] = mComponentManager->GetEntityIndex(migrateEntity);

    ASSERT_EQ(targetArchetype, migratedArchetype);
    ASSERT_TRUE(
        (mComponentManager->HasComponents<PositionComponent, NameComponent>(migrateEntity)));
    ASSERT_FALSE(mComponentManager->HasComponent<ModelComponent>(migrateEntity));
    ASSERT_FLOAT_EQ(mComponentManager->GetComponent<PositionComponent>(migrateEntity).x, 2.f);
    ASSERT_STREQ(mComponentManager->GetComponent<NameComponent>(migrateEntity).name.c_str(),
                 "migrate");
}

TEST_F(ComponentManagerSpec, RemovePartialComponentShouldCreateNewArchetypeWhenNoMatch)
{
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();
    mComponentManager->RegisterComponent<ModelComponent>();

    constexpr fr::Entity entity = 1;

    mComponentManager->AddComponents<PositionComponent, NameComponent, ModelComponent>(
        entity,
        PositionComponent { .x = 10.f },
        NameComponent { .name = "solo" },
        ModelComponent { .mesh = 1, .material = 2, .texture = 3 },
        [](auto, auto&, auto&, auto&) {});

    mRegistry->ExecuteTasks();

    const auto [archetypeBefore, _] = mComponentManager->GetEntityIndex(entity);

    mComponentManager->RemoveComponent<ModelComponent>(entity);
    mRegistry->ExecuteTasks();

    const auto [archetypeAfter, _chunk] = mComponentManager->GetEntityIndex(entity);

    ASSERT_NE(archetypeBefore, archetypeAfter);
    ASSERT_TRUE((mComponentManager->HasComponents<PositionComponent, NameComponent>(entity)));
    ASSERT_FALSE(mComponentManager->HasComponent<ModelComponent>(entity));
    ASSERT_TRUE((archetypeAfter->HasComponents<PositionComponent, NameComponent>()));
    ASSERT_FALSE(archetypeAfter->HasComponent<ModelComponent>());
}

TEST_F(ComponentManagerSpec, RemoveComponentsPartialShouldKeepRemainingComponent)
{
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();
    mComponentManager->RegisterComponent<ModelComponent>();

    constexpr fr::Entity entity = 1;

    mComponentManager->AddComponents<PositionComponent, NameComponent, ModelComponent>(
        entity,
        PositionComponent { .x = 42.f, .y = 7.f, .z = 3.f },
        NameComponent { .name = "partial-remove" },
        ModelComponent { .mesh = 5 },
        [](auto, auto&, auto&, auto&) {});

    mRegistry->ExecuteTasks();

    mComponentManager->RemoveComponents<PositionComponent, ModelComponent>(entity);
    mRegistry->ExecuteTasks();

    ASSERT_FALSE(mComponentManager->HasComponent<PositionComponent>(entity));
    ASSERT_FALSE(mComponentManager->HasComponent<ModelComponent>(entity));
    ASSERT_TRUE(mComponentManager->HasComponent<NameComponent>(entity));
    ASSERT_STREQ(mComponentManager->GetComponent<NameComponent>(entity).name.c_str(),
                 "partial-remove");
    ASSERT_NE(mComponentManager->GetEntityIndex(entity).archetype, nullptr);
}

TEST_F(ComponentManagerSpec, RemoveComponentsPartialShouldMigrateToExistingArchetype)
{
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();
    mComponentManager->RegisterComponent<ModelComponent>();

    constexpr fr::Entity keepEntity    = 1;
    constexpr fr::Entity migrateEntity = 2;

    mComponentManager->AddComponents<NameComponent>(
        keepEntity,
        NameComponent { .name = "target" },
        [](auto, const auto&) {});

    mComponentManager->AddComponents<PositionComponent, NameComponent, ModelComponent>(
        migrateEntity,
        PositionComponent { .x = 1.f },
        NameComponent { .name = "source" },
        ModelComponent { .mesh = 2 },
        [](auto, auto&, auto&, auto&) {});

    mRegistry->ExecuteTasks();

    const auto [targetArchetype, _] = mComponentManager->GetEntityIndex(keepEntity);

    mComponentManager->RemoveComponents<PositionComponent, ModelComponent>(migrateEntity);
    mRegistry->ExecuteTasks();

    const auto [migratedArchetype, _chunk] = mComponentManager->GetEntityIndex(migrateEntity);

    ASSERT_EQ(targetArchetype, migratedArchetype);
    ASSERT_TRUE(mComponentManager->HasComponent<NameComponent>(migrateEntity));
    ASSERT_FALSE(mComponentManager->HasComponent<PositionComponent>(migrateEntity));
    ASSERT_FALSE(mComponentManager->HasComponent<ModelComponent>(migrateEntity));
    ASSERT_STREQ(mComponentManager->GetComponent<NameComponent>(migrateEntity).name.c_str(),
                 "source");
}

TEST_F(ComponentManagerSpec, AddComponentShouldMigrateToNewArchetypeWhenNoMatchDuringUpdate)
{
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();
    mComponentManager->RegisterComponent<ModelComponent>();

    constexpr fr::Entity entity = 1;

    mComponentManager->AddComponents<PositionComponent, NameComponent>(
        entity,
        PositionComponent { .x = 1.f },
        NameComponent { .name = "base" },
        [](auto, auto&, auto&) {});

    mRegistry->ExecuteTasks();

    const auto [archetypeBefore, _] = mComponentManager->GetEntityIndex(entity);

    mComponentManager->AddComponents<ModelComponent>(
        entity,
        ModelComponent { .mesh = 8, .material = 9, .texture = 10 },
        [](auto, const auto&) {});

    mRegistry->ExecuteTasks();

    const auto [archetypeAfter, _chunk] = mComponentManager->GetEntityIndex(entity);

    ASSERT_NE(archetypeBefore, archetypeAfter);
    ASSERT_TRUE((mComponentManager->HasComponents<PositionComponent, NameComponent, ModelComponent>(
        entity)));
    ASSERT_EQ(mComponentManager->GetComponent<ModelComponent>(entity).mesh, 8u);
}

TEST_F(ComponentManagerSpec, MigrationShouldMatchExistingArchetypeAmongMany)
{
    AllTestComponents::RegisterAll(*mComponentManager);

    constexpr fr::Entity positionEntity = 1;
    constexpr fr::Entity velocityEntity = 2;
    constexpr fr::Entity nameEntity     = 3;
    constexpr fr::Entity modelEntity    = 4;
    constexpr fr::Entity decayEntity    = 5;
    constexpr fr::Entity targetEntity   = 6;
    constexpr fr::Entity migrateEntity  = 7;

    mComponentManager->AddComponent(positionEntity, PositionComponent { .x = 1.f });
    mComponentManager->AddComponent(velocityEntity, VelocityComponent { .x = 2.f });
    mComponentManager->AddComponent(nameEntity, NameComponent { .name = "name-only" });
    mComponentManager->AddComponent(modelEntity, ModelComponent { .mesh = 3 });
    mComponentManager->AddComponent(decayEntity, DecayComponent { .timeToLive = 4.f });
    mComponentManager->AddComponents<PositionComponent, NameComponent>(
        targetEntity,
        PositionComponent { .x = 10.f },
        NameComponent { .name = "target" },
        [](auto, auto&, auto&) {});
    mComponentManager->AddComponents<PositionComponent, NameComponent, ModelComponent>(
        migrateEntity,
        PositionComponent { .x = 20.f, .y = 21.f, .z = 22.f },
        NameComponent { .name = "migrate" },
        ModelComponent { .mesh = 30 },
        [](auto, auto&, auto&, auto&) {});

    mRegistry->ExecuteTasks();

    const auto [targetArchetype, _] = mComponentManager->GetEntityIndex(targetEntity);

    mComponentManager->RemoveComponent<ModelComponent>(migrateEntity);
    mRegistry->ExecuteTasks();

    const auto [migratedArchetype, _chunk] = mComponentManager->GetEntityIndex(migrateEntity);

    ASSERT_EQ(targetArchetype, migratedArchetype);
    ASSERT_TRUE(
        (mComponentManager->HasComponents<PositionComponent, NameComponent>(migrateEntity)));
    ASSERT_FALSE(mComponentManager->HasComponent<ModelComponent>(migrateEntity));
    ASSERT_FLOAT_EQ(mComponentManager->GetComponent<PositionComponent>(migrateEntity).x, 20.f);
    ASSERT_STREQ(mComponentManager->GetComponent<NameComponent>(migrateEntity).name.c_str(),
                 "migrate");
}

TEST_F(ComponentManagerSpec, EmptyEntityShouldReuseArchetypeAfterSeveralUnrelatedOnesExist)
{
    AllTestComponents::RegisterAll(*mComponentManager);

    for (fr::Entity entity = 1; entity <= 5; ++entity)
    {
        mComponentManager->AddComponent(
            entity,
            MakeSampleComponent<PositionComponent>(static_cast<int>(entity)));
    }
    mComponentManager->AddComponent(6, MakeSampleComponent<VelocityComponent>(6));
    mComponentManager->AddComponent(7, MakeSampleComponent<NameComponent>(7));
    mComponentManager->AddComponent(8, MakeSampleComponent<ModelComponent>(8));
    mComponentManager->AddComponent(9, MakeSampleComponent<DecayComponent>(9));
    mRegistry->ExecuteTasks();

    const auto [velocityArchetype, _] = mComponentManager->GetEntityIndex(6);

    constexpr fr::Entity lateEntity = 10;
    mComponentManager->AddComponent(lateEntity, MakeSampleComponent<VelocityComponent>(10));
    mRegistry->ExecuteTasks();

    const auto [reusedArchetype, _chunk] = mComponentManager->GetEntityIndex(lateEntity);

    ASSERT_EQ(velocityArchetype, reusedArchetype);
    AssertSampleEquals(mComponentManager->GetComponent<VelocityComponent>(lateEntity),
                       MakeSampleComponent<VelocityComponent>(10));
}

TEST_F(ComponentManagerSpec, RemoveTrailingComponentShouldMigrateCreatingNewArchetype)
{
    AllTestComponents::RegisterAll(*mComponentManager);

    constexpr fr::Entity entity = 1;
    mComponentManager->AddComponents<PositionComponent,
                                     VelocityComponent,
                                     NameComponent,
                                     ModelComponent,
                                     DecayComponent>(
        entity,
        MakeSampleComponent<PositionComponent>(1),
        MakeSampleComponent<VelocityComponent>(1),
        MakeSampleComponent<NameComponent>(1),
        MakeSampleComponent<ModelComponent>(1),
        MakeSampleComponent<DecayComponent>(1),
        [](auto, auto&, auto&, auto&, auto&, auto&) {});
    mRegistry->ExecuteTasks();

    const auto [archetypeBefore, _] = mComponentManager->GetEntityIndex(entity);

    mComponentManager->RemoveComponent<DecayComponent>(entity);
    mRegistry->ExecuteTasks();

    const auto [archetypeAfter, _chunk] = mComponentManager->GetEntityIndex(entity);

    ASSERT_NE(archetypeBefore, archetypeAfter);
    ASSERT_TRUE(
        (mComponentManager
             ->HasComponents<PositionComponent, VelocityComponent, NameComponent, ModelComponent>(
                 entity)));
    ASSERT_FALSE(mComponentManager->HasComponent<DecayComponent>(entity));
    AssertSampleEquals(mComponentManager->GetComponent<PositionComponent>(entity),
                       MakeSampleComponent<PositionComponent>(1));
}

TEST_F(ComponentManagerSpec, RemoveMiddleComponentShouldMigrateToExistingFourPack)
{
    AllTestComponents::RegisterAll(*mComponentManager);

    constexpr fr::Entity keepEntity    = 1;
    constexpr fr::Entity migrateEntity = 2;

    mComponentManager
        ->AddComponents<PositionComponent, VelocityComponent, NameComponent, ModelComponent>(
            keepEntity,
            MakeSampleComponent<PositionComponent>(1),
            MakeSampleComponent<VelocityComponent>(1),
            MakeSampleComponent<NameComponent>(1),
            MakeSampleComponent<ModelComponent>(1),
            [](auto, auto&, auto&, auto&, auto&) {});
    mComponentManager->AddComponents<PositionComponent,
                                     VelocityComponent,
                                     NameComponent,
                                     ModelComponent,
                                     DecayComponent>(
        migrateEntity,
        MakeSampleComponent<PositionComponent>(2),
        MakeSampleComponent<VelocityComponent>(2),
        MakeSampleComponent<NameComponent>(2),
        MakeSampleComponent<ModelComponent>(2),
        MakeSampleComponent<DecayComponent>(2),
        [](auto, auto&, auto&, auto&, auto&, auto&) {});
    mRegistry->ExecuteTasks();

    const auto [targetArchetype, _] = mComponentManager->GetEntityIndex(keepEntity);

    mComponentManager->RemoveComponent<DecayComponent>(migrateEntity);
    mRegistry->ExecuteTasks();

    const auto [migratedArchetype, _chunk] = mComponentManager->GetEntityIndex(migrateEntity);

    ASSERT_EQ(targetArchetype, migratedArchetype);
    ASSERT_FALSE(mComponentManager->HasComponent<DecayComponent>(migrateEntity));
    AssertSampleEquals(mComponentManager->GetComponent<NameComponent>(migrateEntity),
                       MakeSampleComponent<NameComponent>(2));
}

TEST_F(ComponentManagerSpec, LookupShouldReuseExistingArchetypeAmongMany)
{
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<VelocityComponent>();
    mComponentManager->RegisterComponent<NameComponent>();
    mComponentManager->RegisterComponent<ModelComponent>();
    mComponentManager->RegisterComponent<DecayComponent>();

    fr::Entity nextEntity = 100;
    mComponentManager->AddComponent(nextEntity++, MakeSampleComponent<PositionComponent>(1));
    mComponentManager->AddComponents<PositionComponent, VelocityComponent>(
        nextEntity++,
        MakeSampleComponent<PositionComponent>(2),
        MakeSampleComponent<VelocityComponent>(2),
        [](auto, auto&, auto&) {});
    mComponentManager->AddComponents<PositionComponent, NameComponent>(
        nextEntity++,
        MakeSampleComponent<PositionComponent>(3),
        MakeSampleComponent<NameComponent>(3),
        [](auto, auto&, auto&) {});
    mComponentManager->AddComponents<PositionComponent, VelocityComponent, NameComponent>(
        nextEntity++,
        MakeSampleComponent<PositionComponent>(4),
        MakeSampleComponent<VelocityComponent>(4),
        MakeSampleComponent<NameComponent>(4),
        [](auto, auto&, auto&, auto&) {});
    mComponentManager->AddComponents<PositionComponent, ModelComponent>(
        nextEntity++,
        MakeSampleComponent<PositionComponent>(5),
        MakeSampleComponent<ModelComponent>(5),
        [](auto, auto&, auto&) {});
    mComponentManager->AddComponents<PositionComponent,
                                     VelocityComponent,
                                     NameComponent,
                                     ModelComponent,
                                     DecayComponent>(
        nextEntity++,
        MakeSampleComponent<PositionComponent>(6),
        MakeSampleComponent<VelocityComponent>(6),
        MakeSampleComponent<NameComponent>(6),
        MakeSampleComponent<ModelComponent>(6),
        MakeSampleComponent<DecayComponent>(6),
        [](auto, auto&, auto&, auto&, auto&, auto&) {});
    mRegistry->ExecuteTasks();

    constexpr fr::Entity seedEntity   = 100;
    const auto [expectedArchetype, _] = mComponentManager->GetEntityIndex(seedEntity);

    constexpr fr::Entity lookupEntity = 200;
    mComponentManager->AddComponent(lookupEntity, MakeSampleComponent<PositionComponent>(99));
    mRegistry->ExecuteTasks();

    const auto [actualArchetype, _chunk] = mComponentManager->GetEntityIndex(lookupEntity);

    ASSERT_EQ(expectedArchetype, actualArchetype);
    ASSERT_TRUE(mComponentManager->HasComponent<PositionComponent>(lookupEntity));
    ASSERT_FALSE(mComponentManager->HasComponent<VelocityComponent>(lookupEntity));
}

struct LateRegisteredPluginComponent : fr::Component
{
    int value = 0;
};
