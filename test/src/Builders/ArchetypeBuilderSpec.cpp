#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"

class ArchetypeBuilderSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto app = skr::ApplicationBuilder().AddExtension<fr::FreyrExtension>([](Ref<fr::FreyrExtension> freyr) {
            freyr->WithOptions([](fr::FreyrOptionsBuilder& builder) { builder.WithArchetypeChunkCapacity(102); });
        });

        mServiceProvider = app.GetServiceCollection()->CreateServiceProvider();

        mScene = mServiceProvider->GetService<fr::Scene>();
    }

    void TearDown() override { mScene.reset(); }

    Ref<fr::Scene>            mScene;
    Ref<skr::ServiceProvider> mServiceProvider;
};

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilderShouldRegisterComponent)
{
    const auto archetype =
        mScene->CreateArchetypeBuilder()
            .WithComponent(ModelComponent {})
            .WithComponent(PositionComponent {})
            .WithEntities(1)
            .Build();

    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());
    ASSERT_TRUE(archetype->HasComponent<ModelComponent>());
    ASSERT_TRUE((archetype->HasComponents<PositionComponent, ModelComponent>()));
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilderShouldNotHaveUnregisteredComponent)
{
    auto archetype = mScene->CreateArchetypeBuilder().WithComponent(PositionComponent {}).WithEntities(1).Build();

    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());
    ASSERT_FALSE(archetype->HasComponent<ModelComponent>());
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilder_ShouldAddDefaultComponentForAllEntities)
{
    auto archetype = mScene->CreateArchetypeBuilder()
                         .WithComponent(PositionComponent { .x = 100, .y = 100 })
                         .WithEntities(1000)
                         .Build();

    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());

    archetype->ForEach<PositionComponent>("", [this](fr::Entity entity, PositionComponent& position) {
        ASSERT_EQ(position.x, position.y);
        ASSERT_TRUE(mScene->HasComponent<PositionComponent>(entity));
    });
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilder_ShouldAppendDefaultComponentForAllEntities)
{
    const auto archetype =
        mScene->CreateArchetypeBuilder()
            .WithComponent(PositionComponent { .x = 100, .y = 100 })
            .WithEntities(100)
            .Build();

    const auto archetype2 =
        mScene->CreateArchetypeBuilder()
            .WithComponent(PositionComponent { .x = 200, .y = 200 })
            .WithEntities(1000)
            .Build();

    ASSERT_EQ(archetype, archetype2);
    ASSERT_EQ(archetype->Count(), 1100);
    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());

    for (int i = 0; i < 100; ++i)
    {
        auto hasPosition = mScene->TryGetComponents<PositionComponent>(i, [](PositionComponent& position) {
            ASSERT_EQ(position.x, 100);
            ASSERT_EQ(position.y, 100);
        });

        ASSERT_TRUE(hasPosition);
    }

    for (int i = 100; i < 1100; ++i)
    {
        auto hasPosition = mScene->TryGetComponents<PositionComponent>(i, [](PositionComponent& position) {
            ASSERT_EQ(position.x, 200);
            ASSERT_EQ(position.y, 200);
        });

        ASSERT_TRUE(hasPosition);
    }
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilder_ShouldAppendDefaultComponentsForAllEntities)
{
    const auto archetype =
        mScene->CreateArchetypeBuilder()
            .WithComponent(PositionComponent { .x = 100, .y = 100 })
            .WithComponent(NameComponent { .name = "first" })
            .WithEntities(100)
            .Build();

    const auto archetype2 =
        mScene->CreateArchetypeBuilder()
            .WithComponent(PositionComponent { .x = 200, .y = 200 })
            .WithComponent(NameComponent { .name = "second" })
            .WithEntities(1000)
            .Build();

    ASSERT_EQ(archetype, archetype2);
    ASSERT_EQ(archetype->Count(), 1100);
    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());

    for (int i = 0; i < 100; ++i)
    {
        auto hasComponents = mScene->TryGetComponents<PositionComponent, NameComponent>(
            i,
            [](PositionComponent& position, NameComponent& name) {
                ASSERT_EQ(position.x, 100);
                ASSERT_EQ(position.y, 100);
                ASSERT_STREQ(name.name.c_str(), "first");
            });

        ASSERT_TRUE(hasComponents);
    }

    for (int i = 100; i < 1100; ++i)
    {
        auto hasComponents = mScene->TryGetComponents<PositionComponent, NameComponent>(
            i,
            [](PositionComponent& position, NameComponent& name) {
                ASSERT_EQ(position.x, 200);
                ASSERT_EQ(position.y, 200);
                ASSERT_STREQ(name.name.c_str(), "second");
            });

        ASSERT_TRUE(hasComponents);
    }
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilder_ShouldCalculateComponentWithForEach)
{
    const auto archetype =
        mScene->CreateArchetypeBuilder()
            .WithComponent(PositionComponent { .x = 100, .y = 100 })
            .WithComponent(NameComponent {})
            .WithEntities(100)
            .ForEach<NameComponent>([](auto entity, NameComponent& name) { name.name = "first"; })
            .Build();

    const auto archetype2 =
        mScene->CreateArchetypeBuilder()
            .WithComponent(PositionComponent { .x = 200, .y = 200 })
            .WithComponent(NameComponent {})
            .ForEach<NameComponent>([](auto entity, NameComponent& name) { name.name = "second"; })
            .WithEntities(1000)
            .Build();

    ASSERT_EQ(archetype, archetype2);
    ASSERT_EQ(archetype->Count(), 1100);
    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());
    for (int i = 0; i < 100; ++i)
    {
        auto hasComponents = mScene->TryGetComponents<PositionComponent, NameComponent>(
            i,
            [](PositionComponent& position, NameComponent& name) {
                ASSERT_EQ(position.x, 100);
                ASSERT_EQ(position.y, 100);
                ASSERT_STREQ(name.name.c_str(), "first");
            });

        ASSERT_TRUE(hasComponents);
    }

    for (int i = 100; i < 1100; ++i)
    {
        auto hasComponents = mScene->TryGetComponents<PositionComponent, NameComponent>(
            i,
            [](PositionComponent& position, NameComponent& name) {
                ASSERT_EQ(position.x, 200);
                ASSERT_EQ(position.y, 200);
                ASSERT_STREQ(name.name.c_str(), "second");
            });

        ASSERT_TRUE(hasComponents);
    }
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilder_ShouldBuildArchetypeCorrectlyWithExistingEntities)
{
    mScene->CreateEntity(PositionComponent { .x = 300, .y = 300 }, NameComponent { .name = "first" });

    const auto archetype =
        mScene->CreateArchetypeBuilder()
            .WithComponent(PositionComponent { .x = 100, .y = 100 })
            .WithComponent(NameComponent {})
            .WithEntities(100)
            .ForEach<NameComponent>([](auto entity, NameComponent& name) { name.name = "second"; })
            .Build();

    const auto archetype2 =
        mScene->CreateArchetypeBuilder()
            .WithComponent(PositionComponent { .x = 200, .y = 200 })
            .WithComponent(NameComponent {})
            .ForEach<NameComponent>([](auto entity, NameComponent& name) { name.name = "third"; })
            .WithEntities(1000)
            .Build();

    ASSERT_EQ(archetype, archetype2);
    ASSERT_EQ(archetype->Count(), 1101);
    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());

    // ASSERT_EQ(archetype->GetComponent<PositionComponent>(0).x, 300);
    // ASSERT_EQ(archetype->GetComponent<PositionComponent>(0).y, 300);
    // ASSERT_STREQ(archetype->GetComponent<NameComponent>(0).name.c_str(),
    //              "first");

    for (int i = 1; i <= 100; ++i)
    {
        auto hasComponents = mScene->TryGetComponents<PositionComponent, NameComponent>(
            i,
            [](PositionComponent& position, NameComponent& name) {
                ASSERT_EQ(position.x, 100);
                ASSERT_EQ(position.y, 100);
                ASSERT_STREQ(name.name.c_str(), "second");
            });

        ASSERT_TRUE(hasComponents);
    }

    for (int i = 101; i <= 1100; ++i)
    {
        auto hasComponents = mScene->TryGetComponents<PositionComponent, NameComponent>(
            i,
            [](PositionComponent& position, NameComponent& name) {
                ASSERT_EQ(position.x, 200);
                ASSERT_EQ(position.y, 200);
                ASSERT_STREQ(name.name.c_str(), "third");
            });

        ASSERT_TRUE(hasComponents);
    }
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilder_ShouldBuildEntitiesThatCanBeMovedToAnotherArchetype)
{
    // Arrange
    const auto archetype =
        mScene->CreateArchetypeBuilder()
            .WithComponent(NameComponent {})
            .WithEntities(10)
            .ForEach<NameComponent>([](auto entity, NameComponent& name) {
                std::stringstream ss;
                ss << "Entity(" << entity << ")";
                name.name = ss.str();
            })
            .Build();

    // Act
    mScene->AddComponent(8, PositionComponent { .x = 100, .y = 100 });
    mScene->ExecuteTasks();

    // Assert
    ASSERT_EQ(archetype->Count(), 9);
    ASSERT_TRUE(archetype->HasComponent<NameComponent>());

    auto has = mScene->TryGetComponents<PositionComponent, NameComponent>(
        8,
        [](PositionComponent& position, NameComponent& name) {
            ASSERT_EQ(position.x, 100);
            ASSERT_EQ(position.y, 100);
            ASSERT_STREQ(name.name.c_str(), "Entity(8)");
        });
    ASSERT_TRUE(has);

    mScene->CreateQuery()->Each<NameComponent>([](auto entity, NameComponent& name) {
        std::stringstream ss;
        ss << "Entity(" << entity << ")";
        ASSERT_STREQ(name.name.c_str(), ss.str().c_str());
    });
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilder_ShouldReturnNullWithNoEntities)
{
    // Arrange
    auto builder = mScene->CreateArchetypeBuilder().WithComponent(NameComponent {}).WithEntities(0);

    // Act
    const auto archetype = builder.Build();

    // Assert
    ASSERT_EQ(archetype, nullptr);
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilder_ShouldNotRegisterDuplicateComponent)
{
    mScene->CreateArchetypeBuilder().WithComponent(PositionComponent {}).WithEntities(1).Build();

    const auto archetype =
        mScene->CreateArchetypeBuilder()
            .WithComponent(PositionComponent {})
            .WithComponent(NameComponent {})
            .WithEntities(10)
            .Build();

    ASSERT_NE(archetype, nullptr);
    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());
    ASSERT_TRUE(archetype->HasComponent<NameComponent>());
}
