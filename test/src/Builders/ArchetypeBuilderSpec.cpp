#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "Freyr/Builders/SceneBuilder.hpp"

class ArchetypeBuilderSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mScene = fr::SceneBuilder()
                     .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                         builder.SetInitialCapacity(10000);
                     })
                     .Build();
    }

    void TearDown() override { mScene.reset(); }

    Ref<fr::Scene> mScene;
};

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilderShouldRegisterComponent)
{
    const auto archetype =
        mScene->CreateArchetypeBuilder()
            .WithDefault(ModelComponent {})
            .WithDefault(PositionComponent {})
            .WithEntities(1)
            .Build();

    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());
    ASSERT_TRUE(archetype->HasComponent<ModelComponent>());
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilderShouldNotHaveUnregisteredComponent)
{
    auto archetype = mScene->CreateArchetypeBuilder()
                         .WithDefault(PositionComponent {})
                         .WithEntities(1)
                         .Build();

    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());
    ASSERT_FALSE(archetype->HasComponent<ModelComponent>());
}

TEST_F(ArchetypeBuilderSpec,
       ArchetypeBuilder_ShouldAddDefaultComponentForAllEntities)
{
    auto archetype = mScene->CreateArchetypeBuilder()
                         .WithDefault(PositionComponent { .x = 100, .y = 100 })
                         .WithEntities(1000)
                         .Build();

    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());

    archetype->ForEach<PositionComponent>(
        "",
        [this](fr::Entity entity, PositionComponent& position) {
            ASSERT_EQ(position.x, position.y);
            ASSERT_TRUE(mScene->HasComponent<PositionComponent>(entity));
        });
}

TEST_F(ArchetypeBuilderSpec,
       ArchetypeBuilder_ShouldAppendDefaultComponentForAllEntities)
{
    auto archetype = mScene->CreateArchetypeBuilder()
                         .WithDefault(PositionComponent { .x = 100, .y = 100 })
                         .WithEntities(100)
                         .Build();

    auto archetype2 =
        mScene->CreateArchetypeBuilder()
            .WithDefault(PositionComponent { .x = 200, .y = 200 })
            .WithEntities(1000)
            .Build();

    ASSERT_EQ(archetype, archetype2);
    ASSERT_EQ(archetype->Count(), 1100);
    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());

    for (int i = 0; i < 100; ++i)
    {
        ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).x, 100);
        ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).y, 100);
    }

    for (int i = 100; i < 1100; ++i)
    {
        ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).x, 200);
        ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).y, 200);
    }
}