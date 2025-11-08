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
        auto app = skr::ApplicationBuilder().AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
            freyr.WithOptions([](fr::FreyrOptionsBuilder& builder) { builder.SetArchetypeChunkCapacity(102); });
        });

        const auto provider = app.GetServiceCollection().CreateServiceProvider();

        mScene = provider->GetService<fr::Scene>();
    }

    void TearDown() override { mScene.reset(); }

    Ref<fr::Scene> mScene;
};

// TODO: refactor

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilderShouldRegisterComponent)
{
    // const auto archetype =
    //     mScene->CreateArchetypeBuilder()
    //         .WithDefault(ModelComponent {})
    //         .WithDefault(PositionComponent {})
    //         .WithEntities(1)
    //         .Build();
    //
    // ASSERT_TRUE(archetype->HasComponent<PositionComponent>());
    // ASSERT_TRUE(archetype->HasComponent<ModelComponent>());
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilderShouldNotHaveUnregisteredComponent)
{
    // auto archetype = mScene->CreateArchetypeBuilder()
    //                      .WithDefault(PositionComponent {})
    //                      .WithEntities(1)
    //                      .Build();
    //
    // ASSERT_TRUE(archetype->HasComponent<PositionComponent>());
    // ASSERT_FALSE(archetype->HasComponent<ModelComponent>());
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilder_ShouldAddDefaultComponentForAllEntities)
{
    // auto archetype = mScene->CreateArchetypeBuilder()
    //                      .WithDefault(PositionComponent { .x = 100, .y = 100 })
    //                      .WithEntities(1000)
    //                      .Build();
    //
    // ASSERT_TRUE(archetype->HasComponent<PositionComponent>());
    //
    // archetype->ForEach<PositionComponent>(
    //     "",
    //     [this](fr::Entity entity, PositionComponent& position) {
    //         ASSERT_EQ(position.x, position.y);
    //         ASSERT_TRUE(mScene->HasComponent<PositionComponent>(entity));
    //     });
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilder_ShouldAppendDefaultComponentForAllEntities)
{
    // const auto archetype =
    //     mScene->CreateArchetypeBuilder()
    //         .WithDefault(PositionComponent { .x = 100, .y = 100 })
    //         .WithEntities(100)
    //         .Build();
    //
    // const auto archetype2 =
    //     mScene->CreateArchetypeBuilder()
    //         .WithDefault(PositionComponent { .x = 200, .y = 200 })
    //         .WithEntities(1000)
    //         .Build();
    //
    // ASSERT_EQ(archetype, archetype2);
    // ASSERT_EQ(archetype->Count(), 1100);
    // ASSERT_TRUE(archetype->HasComponent<PositionComponent>());
    //
    // for (int i = 0; i < 100; ++i)
    // {
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).x, 100);
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).y, 100);
    // }
    //
    // for (int i = 100; i < 1100; ++i)
    // {
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).x, 200);
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).y, 200);
    // }
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilder_ShouldAppendDefaultComponentsForAllEntities)
{
    // const auto archetype =
    //     mScene->CreateArchetypeBuilder()
    //         .WithDefault(PositionComponent { .x = 100, .y = 100 })
    //         .WithDefault(NameComponent { .name = "first" })
    //         .WithEntities(100)
    //         .Build();
    //
    // const auto archetype2 =
    //     mScene->CreateArchetypeBuilder()
    //         .WithDefault(PositionComponent { .x = 200, .y = 200 })
    //         .WithDefault(NameComponent { .name = "second" })
    //         .WithEntities(1000)
    //         .Build();
    //
    // ASSERT_EQ(archetype, archetype2);
    // ASSERT_EQ(archetype->Count(), 1100);
    // ASSERT_TRUE(archetype->HasComponent<PositionComponent>());

    // for (int i = 0; i < 100; ++i)
    // {
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).x, 100);
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).y, 100);
    //     ASSERT_STREQ(archetype->GetComponent<NameComponent>(i).name.c_str(),
    //                  "first");
    // }
    //
    // for (int i = 100; i < 1100; ++i)
    // {
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).x, 200);
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).y, 200);
    //     ASSERT_STREQ(archetype->GetComponent<NameComponent>(i).name.c_str(),
    //                  "second");
    // }
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilder_ShouldCalculateComponentWithForEach)
{
    // const auto archetype =
    //     mScene->CreateArchetypeBuilder()
    //         .WithDefault(PositionComponent { .x = 100, .y = 100 })
    //         .WithDefault(NameComponent {})
    //         .WithEntities(100)
    //         .ForEach<NameComponent>([](auto entity, NameComponent& name) {
    //             name.name = "first";
    //         })
    //         .Build();
    //
    // const auto archetype2 =
    //     mScene->CreateArchetypeBuilder()
    //         .WithDefault(PositionComponent { .x = 200, .y = 200 })
    //         .WithDefault(NameComponent {})
    //         .ForEach<NameComponent>([](auto entity, NameComponent& name) {
    //             name.name = "second";
    //         })
    //         .WithEntities(1000)
    //         .Build();
    //
    // ASSERT_EQ(archetype, archetype2);
    // ASSERT_EQ(archetype->Count(), 1100);
    // ASSERT_TRUE(archetype->HasComponent<PositionComponent>());
    // for (int i = 0; i < 100; ++i)
    // {
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).x, 100);
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).y, 100);
    //     ASSERT_STREQ(archetype->GetComponent<NameComponent>(i).name.c_str(),
    //                  "first");
    // }
    //
    // for (int i = 100; i < 1100; ++i)
    // {
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).x, 200);
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).y, 200);
    //     ASSERT_STREQ(archetype->GetComponent<NameComponent>(i).name.c_str(),
    //                  "second");
    // }
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilder_ShouldBuildArchetypeCorrectlyWithExistingEntities)
{
    // const auto firstEntity = mScene->CreateEntity();
    // mScene->AddComponent(firstEntity, PositionComponent { .x = 300, .y = 300 });
    // mScene->AddComponent(firstEntity, NameComponent { .name = "first" });
    //
    // const auto archetype =
    //     mScene->CreateArchetypeBuilder()
    //         .WithDefault(PositionComponent { .x = 100, .y = 100 })
    //         .WithDefault(NameComponent {})
    //         .WithEntities(100)
    //         .ForEach<NameComponent>([](auto entity, NameComponent& name) {
    //             name.name = "second";
    //         })
    //         .Build();
    //
    // const auto archetype2 =
    //     mScene->CreateArchetypeBuilder()
    //         .WithDefault(PositionComponent { .x = 200, .y = 200 })
    //         .WithDefault(NameComponent {})
    //         .ForEach<NameComponent>([](auto entity, NameComponent& name) {
    //             name.name = "third";
    //         })
    //         .WithEntities(1000)
    //         .Build();

    // ASSERT_EQ(archetype, archetype2);
    // ASSERT_EQ(archetype->Count(), 1101);
    // ASSERT_TRUE(archetype->HasComponent<PositionComponent>());

    // ASSERT_EQ(archetype->GetComponent<PositionComponent>(0).x, 300);
    // ASSERT_EQ(archetype->GetComponent<PositionComponent>(0).y, 300);
    // ASSERT_STREQ(archetype->GetComponent<NameComponent>(0).name.c_str(),
    //              "first");
    //
    // for (int i = 1; i <= 100; ++i)
    // {
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).x, 100);
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).y, 100);
    //     ASSERT_STREQ(archetype->GetComponent<NameComponent>(i).name.c_str(),
    //                  "second");
    // }
    //
    // for (int i = 101; i <= 1100; ++i)
    // {
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).x, 200);
    //     ASSERT_EQ(archetype->GetComponent<PositionComponent>(i).y, 200);
    //     ASSERT_STREQ(archetype->GetComponent<NameComponent>(i).name.c_str(),
    //                  "third");
    // }
}

TEST_F(ArchetypeBuilderSpec, ArchetypeBuilder_ShouldBuildEntitiesThatCanBeMovedToAnotherArchetype)
{
    // const auto archetype =
    //     mScene->CreateArchetypeBuilder()
    //         .WithDefault(NameComponent {})
    //         .WithEntities(10)
    //         .ForEach<NameComponent>([](auto entity, NameComponent& name) {
    //             std::stringstream ss;
    //             ss << "Entity(" << entity << ")";
    //             name.name = ss.str();
    //         })
    //         .Build();
    //
    // mScene->AddComponent(8, PositionComponent { .x = 100, .y = 100 });

    // ASSERT_EQ(archetype->Count(), 9);
    // ASSERT_TRUE(archetype->HasComponent<NameComponent>());
    //
    // auto has = mScene->TryGetComponents<PositionComponent, NameComponent>(
    //     8,
    //     [](PositionComponent& position, NameComponent& name) {
    //         ASSERT_EQ(position.x, 100);
    //         ASSERT_EQ(position.y, 100);
    //         ASSERT_STREQ(name.name.c_str(), "Entity(8)");
    //     });
    // ASSERT_TRUE(has);
    //
    // mScene->ForEach<NameComponent>([](auto entity, NameComponent& name) {
    //     std::stringstream ss;
    //     ss << "Entity(" << entity << ")";
    //     ASSERT_STREQ(name.name.c_str(), ss.str().c_str());
    // });
}