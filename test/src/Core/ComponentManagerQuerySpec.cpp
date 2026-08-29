#include "ComponentManagerTestSupport.hpp"

#include "../Components/DecayComponent.hpp"
#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "../Components/VelocityComponent.hpp"

TEST_F(ComponentManagerSpec, ComponentManagerShouldReturnFalseWhenEntityDoesNotExists)
{
    // Arrange

    // Act
    const auto hasSingle   = mComponentManager->HasComponent<NameComponent>(1);
    const auto hasMultiple = mComponentManager->HasComponents<NameComponent, PositionComponent>(1);
    const auto couldGetInexistentComponent =
        mComponentManager->TryGetComponents<NameComponent, PositionComponent>(1, [](auto&, auto&) {
        });

    // Assert
    ASSERT_FALSE(hasSingle);
    ASSERT_FALSE(hasMultiple);
    ASSERT_FALSE(couldGetInexistentComponent);
}

TEST_F(ComponentManagerSpec, ComponentManagerShouldReturnFalseWhenEntityDoesNotHaveTheComponents)
{
    // Arrange
    mComponentManager->AddComponent(1, NameComponent { .name = "First Entity" });
    mRegistry->ExecuteTasks();

    // Act
    const auto hasSingle   = mComponentManager->HasComponent<NameComponent>(1);
    const auto hasMultiple = mComponentManager->HasComponents<NameComponent, PositionComponent>(1);
    const auto couldGetInexistentComponent =
        mComponentManager->TryGetComponents<NameComponent, PositionComponent>(1, [](auto&, auto&) {
        });

    // Assert
    ASSERT_TRUE(hasSingle);
    ASSERT_FALSE(hasMultiple);
    ASSERT_FALSE(couldGetInexistentComponent);
}

TEST_F(ComponentManagerSpec, ForEachShouldVisitMatchingEntitiesAndSkipUnrelatedArchetypes)
{
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();

    mComponentManager->AddComponent(1, PositionComponent { .x = 10.f });
    mComponentManager->AddComponent(2, PositionComponent { .x = 20.f });
    mComponentManager->AddComponents<NameComponent>(
        3,
        NameComponent { .name = "skip-me" },
        [](auto, const auto&) {});
    mRegistry->ExecuteTasks();

    std::size_t visited = 0;
    float       sumX    = 0.f;
    mComponentManager->ForEach<PositionComponent>(
        "ForEachCoverage",
        [&](fr::Entity, PositionComponent& position) {
            ++visited;
            sumX += position.x;
        });

    ASSERT_EQ(visited, 2u);
    ASSERT_FLOAT_EQ(sumX, 30.f);

    std::size_t nameVisited = 0;
    mComponentManager->ForEach<NameComponent>("ForEachNameCoverage",
                                              [&](fr::Entity, NameComponent&) { ++nameVisited; });
    ASSERT_EQ(nameVisited, 1u);

    std::size_t bothVisited = 0;
    mComponentManager->ForEach<PositionComponent, NameComponent>(
        "ForEachBothCoverage",
        [&](fr::Entity, PositionComponent&, NameComponent&) { ++bothVisited; });
    ASSERT_EQ(bothVisited, 0u);
}

TEST_F(ComponentManagerSpec, ComponentManagerForEachShouldMatchQueryCount)
{
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<VelocityComponent>();

    mComponentManager->AddComponent(1, PositionComponent { .x = 1.f });
    mComponentManager->AddComponent(2, PositionComponent { .x = 2.f });
    mComponentManager->AddComponents<PositionComponent, VelocityComponent>(
        3,
        PositionComponent { .x = 3.f },
        VelocityComponent { .x = 1.f });
    mRegistry->ExecuteTasks();

    const auto queryCount = mRegistry->CreateQuery()->Count<PositionComponent>();

    std::size_t forEachCount = 0;
    mComponentManager->ForEach<PositionComponent>(
        "QueryParity",
        [&](fr::Entity, PositionComponent&) { ++forEachCount; });

    ASSERT_EQ(queryCount, forEachCount);
    ASSERT_EQ(queryCount, 3u);
}

TEST_F(ComponentManagerSpec, HasAndTryGetShouldFailWhenEntityLacksRequestedComponent)
{
    mComponentManager->RegisterComponent<PositionComponent>();
    mComponentManager->RegisterComponent<NameComponent>();

    mComponentManager->AddComponent(1, PositionComponent { .x = 1.f });
    mRegistry->ExecuteTasks();

    ASSERT_TRUE(mComponentManager->HasComponent<PositionComponent>(1));
    ASSERT_FALSE(mComponentManager->HasComponent<NameComponent>(1));
    ASSERT_FALSE((mComponentManager->HasComponents<PositionComponent, NameComponent>(1)));
    ASSERT_FALSE((mComponentManager->TryGetComponents<PositionComponent, NameComponent>(
        1,
        [](PositionComponent&, NameComponent&) {})));
}
