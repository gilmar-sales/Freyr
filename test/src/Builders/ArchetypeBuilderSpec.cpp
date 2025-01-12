#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/PositionComponent.hpp"

class ArchetypeBuilderSpec : public ::testing::Test
{
  protected:
    void SetUp() override { mScene = std::make_shared<fr::Scene>(10000); }

    void TearDown() override { mScene.reset(); }

    std::shared_ptr<fr::Scene> mScene;
};

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilderShouldRegisterComponent)
{
    auto archetype = mScene->CreateArchetypeBuilder()
                         .WithDefault(ModelComponent {})
                         .WithDefault(PositionComponent {})
                         .Build(1);

    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());
    ASSERT_TRUE(archetype->HasComponent<ModelComponent>());
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilderShouldNotHaveUnregisteredComponent)
{
    auto archetype = mScene->CreateArchetypeBuilder()
                         .WithDefault(PositionComponent {})
                         .Build(1);

    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());
    ASSERT_FALSE(archetype->HasComponent<ModelComponent>());
}

TEST_F(ArchetypeBuilderSpec,
       ArchetypeBuilder_ShouldAddDefaultComponentForAllEntities)
{
    auto archetype = mScene->CreateArchetypeBuilder()
                         .WithDefault(PositionComponent { .x = 100, .y = 100 })
                         .Build(1000);

    ASSERT_TRUE(archetype->HasComponent<PositionComponent>());

    archetype->ForEach<PositionComponent>(
        "",
        [=](fr::Entity entity, PositionComponent& position) {
            ASSERT_EQ(position.x, position.y);
            ASSERT_TRUE(mScene->HasComponent<PositionComponent>(entity));
        });
}

TEST_F(ArchetypeBuilderSpec,
       ArchetypeBuilder_ShouldAppendDefaultComponentForAllEntities)
{
    auto archetype = mScene->CreateArchetypeBuilder()
                         .WithDefault(PositionComponent { .x = 100, .y = 100 })
                         .Build(100);

    auto archetype2 = mScene->CreateArchetypeBuilder()
                          .WithDefault(PositionComponent { .x = 200, .y = 200 })
                          .Build(1000);

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