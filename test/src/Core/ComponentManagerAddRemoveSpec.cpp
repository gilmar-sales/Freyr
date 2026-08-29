#include "ComponentManagerTestSupport.hpp"

#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "../Components/VelocityComponent.hpp"
#include "../Components/DecayComponent.hpp"

TEST_F(ComponentManagerSpec, ComponentManager_ShouldAddSingleComponent)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();

    // Act
    for (auto i = 0; i < kComponentManagerEntityCount; i++)
        mComponentManager->AddComponent(i, PositionComponent { .x = static_cast<float>(i) });

    mRegistry->ExecuteTasks();

    // Assert
    for (auto i = 0; i < kComponentManagerEntityCount; i++)
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
    for (unsigned i = 0; i < kComponentManagerEntityCount; i++)
        mComponentManager->AddComponents<PositionComponent, ModelComponent>(
            i,
            PositionComponent { .x = static_cast<float>(i) },
            ModelComponent { .mesh = i, .material = i, .texture = i });

    mRegistry->ExecuteTasks();

    // Assert
    for (auto i = 0; i < kComponentManagerEntityCount; i++)
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

    for (auto i = 0; i < kComponentManagerEntityCount; i++)
        mComponentManager->AddComponent(i, PositionComponent { .x = static_cast<float>(i) });

    mRegistry->ExecuteTasks();

    // Act
    for (auto i = 0; i < kComponentManagerEntityCount; i++)
        mComponentManager->EntityDestroyed(i);

    mRegistry->ExecuteTasks();

    // Assert
    for (auto i = 0; i < kComponentManagerEntityCount; i++)
        ASSERT_FALSE(mComponentManager->HasComponent<PositionComponent>(i));
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldRemoveComponent)
{
    // Arrange
    mComponentManager->RegisterComponent<PositionComponent>();

    for (auto i = 0; i < kComponentManagerEntityCount; i++)
        mComponentManager->AddComponent(i, PositionComponent { .x = static_cast<float>(i) });

    mRegistry->ExecuteTasks();

    // Act
    for (auto i = 0; i < kComponentManagerEntityCount; i++)
        mComponentManager->RemoveComponent<PositionComponent>(i);

    mRegistry->ExecuteTasks();

    // Assert
    for (auto i = 0; i < kComponentManagerEntityCount; i++)
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

    mRegistry->ExecuteTasks();

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
    for (auto i = 0; i < kComponentManagerEntityCount; i++)
        mComponentManager->AddComponent(i, PositionComponent { .x = static_cast<float>(i) });
    mComponentManager->AddComponent(0, PositionComponent { .x = 1000.0f });

    mRegistry->ExecuteTasks();

    // Assert
    for (auto i = 1; i < kComponentManagerEntityCount; i++)
        ASSERT_EQ(mComponentManager->GetComponent<PositionComponent>(i).x, static_cast<float>(i));
    ASSERT_EQ(mComponentManager->GetComponent<PositionComponent>(0).x, 1000.0f);
}

TEST_F(ComponentManagerSpec, CreateEntityWithoutComponentsShouldReturnEmptyEntity)
{
    const auto entity = mRegistry->CreateEntity();

    ASSERT_EQ(mComponentManager->GetEntityIndex(entity).archetype, nullptr);
    ASSERT_EQ(mComponentManager->GetEntityIndex(entity).archetypeChunk, nullptr);
    ASSERT_FALSE(mComponentManager->HasComponent<PositionComponent>(entity));
}

TEST_F(ComponentManagerSpec, EntityDestroyedShouldClearIndexForEntityWithoutComponents)
{
    const auto entity = mRegistry->CreateEntity();

    ASSERT_EQ(mComponentManager->GetEntityIndex(entity).archetype, nullptr);
    ASSERT_EQ(mComponentManager->GetEntityIndex(entity).archetypeChunk, nullptr);

    mComponentManager->EntityDestroyed(entity);

    ASSERT_EQ(mComponentManager->GetEntityIndex(entity).archetype, nullptr);
    ASSERT_EQ(mComponentManager->GetEntityIndex(entity).archetypeChunk, nullptr);
    ASSERT_FALSE(mComponentManager->HasComponent<PositionComponent>(entity));
}

TEST_F(ComponentManagerSpec, RemoveComponentOnEmptyEntityShouldBeNoOp)
{
    AllTestComponents::RegisterAll(*mComponentManager);

    const auto entity = mRegistry->CreateEntity();
    AllTestComponents::ExerciseRemoveOnEmptyEntity(*mComponentManager, *mRegistry, entity);
}

TEST_F(ComponentManagerSpec, RemoveComponentsShouldClearAllRequestedComponents)
{
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();
    mComponentManager->RegisterComponent<ModelComponent>();

    const auto entity = mRegistry->CreateEntity(PositionComponent { .x = 1.f },
                                                NameComponent { .name = "multi-remove" },
                                                ModelComponent { .mesh = 1, .material = 2, .texture = 3 });
    mRegistry->ExecuteTasks();

    ASSERT_TRUE((mComponentManager->HasComponents<PositionComponent, NameComponent, ModelComponent>(entity)));

    mRegistry->RemoveComponents<PositionComponent, NameComponent, ModelComponent>(entity);
    mRegistry->ExecuteTasks();

    ASSERT_FALSE(mComponentManager->HasComponent<PositionComponent>(entity));
    ASSERT_FALSE(mComponentManager->HasComponent<NameComponent>(entity));
    ASSERT_FALSE(mComponentManager->HasComponent<ModelComponent>(entity));
    ASSERT_EQ(mComponentManager->GetEntityIndex(entity).archetype, nullptr);
}

TEST_F(ComponentManagerSpec, ComponentPackShouldCoverSingularPathsForAllTestComponents)
{
    AllTestComponents::RegisterAll(*mComponentManager);
    AllTestComponents::ExerciseSingularLifecycle(*mComponentManager, *mRegistry, 200);
}

TEST_F(ComponentManagerSpec, ComponentPackShouldCoverMultiAddRemoveForCoreComponents)
{
    CoreTestComponents::RegisterAll(*mComponentManager);
    CoreTestComponents::ExerciseMultiAddRemove(*mComponentManager, *mRegistry, 300);
}

TEST_F(ComponentManagerSpec, AddComponentsWithoutCallbackShouldSupportSingleAndTriplePacks)
{
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();
    mComponentManager->RegisterComponent<ModelComponent>();
    mComponentManager->RegisterComponent<VelocityComponent>();

    constexpr fr::Entity singleEntity = 1;
    constexpr fr::Entity tripleEntity = 2;

    mComponentManager->AddComponents<VelocityComponent>(singleEntity, VelocityComponent { .x = 1.f, .y = 2.f });
    mComponentManager->AddComponents<PositionComponent, NameComponent, ModelComponent>(
        tripleEntity,
        PositionComponent { .x = 3.f, .y = 4.f, .z = 5.f },
        NameComponent { .name = "triple-no-callback" },
        ModelComponent { .mesh = 7, .material = 8, .texture = 9 });

    mRegistry->ExecuteTasks();

    ASSERT_TRUE(mComponentManager->HasComponent<VelocityComponent>(singleEntity));
    ASSERT_FLOAT_EQ(mComponentManager->GetComponent<VelocityComponent>(singleEntity).x, 1.f);
    ASSERT_TRUE((mComponentManager->HasComponents<PositionComponent, NameComponent, ModelComponent>(tripleEntity)));
    ASSERT_STREQ(mComponentManager->GetComponent<NameComponent>(tripleEntity).name.c_str(), "triple-no-callback");
    ASSERT_EQ(mComponentManager->GetComponent<ModelComponent>(tripleEntity).mesh, 7u);
}

