#include "../Components/DecayComponent.hpp"

#include "gtest/gtest.h"
#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "../Systems/DecaySystem.hpp"
#include "../Systems/MovementSystem.hpp"

class App : public skr::IApplication
{
  public:
    App(const Ref<skr::ServiceProvider>& rootServiceProvider) : IApplication(rootServiceProvider) {}

    void Run() override {}
};

class SceneSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto app =
            skr::ApplicationBuilder()
                .AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                    freyr.AddComponent<PositionComponent>()
                        .AddComponent<ModelComponent>()
                        .AddComponent<DecayComponent>()
                        .AddSystem<DecaySystem>()
                        .AddSystem<MovementSystem>()
                        .WithOptions([](fr::FreyrOptionsBuilder& builder) { builder.SetArchetypeChunkCapacity(2048); });
                })
                .Build<App>();

        auto& provider = app->GetRootServiceProvider();

        mScene = provider.GetService<fr::Scene>();
    }

    void TearDown() override { mScene.reset(); }

    Ref<fr::Scene> mScene;
};

TEST_F(SceneSpec, SceneShouldTryGetSingleComponent)
{
    // Arrange
    mScene->CreateEntity([&](auto entity) {
        // Act
        mScene->AddComponent(entity, PositionComponent { .x = 100 });

        // Assert
        ASSERT_TRUE(mScene->TryGetComponents<PositionComponent>(entity, [](PositionComponent& position) {
            ASSERT_EQ(position.x, 100);
        }));
    });
}

TEST_F(SceneSpec, SceneShouldAddMultipleComponentsKeepingValues)
{
    // Arrange
    mScene->CreateEntity([&](auto entity) {
        // Act
        mScene->AddComponent(entity, PositionComponent { .x = 100 });
        mScene->AddComponent(entity, ModelComponent { .mesh = 200 });

        // Assert
        auto has = mScene->TryGetComponents<PositionComponent, ModelComponent>(
            entity,
            [](PositionComponent& position, ModelComponent& model) {
                ASSERT_EQ(position.x, 100);
                ASSERT_EQ(model.mesh, 200);
            });
        ASSERT_TRUE(has);
    });
}

TEST_F(SceneSpec, SceneShouldAddMultipleComponentsAtOnceKeepingValues)
{
    // Arrange
    mScene->CreateEntity([&](auto entity) {
        // Act
        mScene->AddComponents(entity, PositionComponent { .x = 100 }, ModelComponent { .mesh = 200 });

        // Assert
        auto has = mScene->TryGetComponents<PositionComponent, ModelComponent>(
            entity,
            [](PositionComponent& position, ModelComponent& model) {
                ASSERT_EQ(position.x, 100);
                ASSERT_EQ(model.mesh, 200);
            });
        ASSERT_TRUE(has);
    });
}

TEST_F(SceneSpec, SceneShouldFindUnique)
{
    // Arrange
    for (auto i = 0; i < 2000; i++)
    {
        mScene->CreateEntity(PositionComponent {});
    }

    const auto modelEntity = static_cast<fr::Entity>(987);
    mScene->AddComponent(modelEntity, ModelComponent {});

    // Act
    const auto unique = mScene->FindUnique<PositionComponent, ModelComponent>();

    // Assert
    ASSERT_EQ(unique, modelEntity);
}

TEST_F(SceneSpec, SceneShouldBeAbleToCreateAndDestroyEntitiesWhileUpdating)
{
    // Arrange
    for (auto i = 0; i < 2000; i++)
    {
        mScene->CreateEntity(PositionComponent {});
    }

    // Act
    for (auto i = 0; i < 10; i++)
        mScene->Update(0.016f);

    // Assert
    ASSERT_EQ(mScene->Count<PositionComponent>(), 2000);
}