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
        mApp = skr::ApplicationBuilder()
                   .AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>()
                           .WithComponent<ModelComponent>()
                           .WithComponent<DecayComponent>()
                           .WithSystem<DecaySystem>()
                           .WithSystem<MovementSystem>()
                           .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                               builder.WithArchetypeChunkCapacity(2048);
                           });
                   })
                   .Build<App>();

        mScene = mApp->GetRootServiceProvider().GetService<fr::Scene>();
    }

    void TearDown() override
    {
        mScene.reset();
        mApp.reset();
    }

    Ref<fr::Scene> mScene;
    Ref<App>       mApp;
};

TEST_F(SceneSpec, SceneShouldTryGetSingleComponent)
{
    // Arrange
    mScene->CreateEntity([&](auto entity) {
        // Act
        mScene->AddComponent(entity, PositionComponent { .x = 100 });
        mScene->ExecuteTasks();

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
        mScene->ExecuteTasks();

        // Assert
        auto has = mScene->TryGetComponents<PositionComponent, ModelComponent>(
            entity,
            [](const PositionComponent& position, const ModelComponent& model) {
                ASSERT_EQ(position.x, 100);
                ASSERT_EQ(model.mesh, 200);
            });
        ASSERT_TRUE(has);
    });
}

TEST_F(SceneSpec, SceneShouldAddMultipleComponentsAtOnceKeepingValues)
{
    // Arrange
    fr::Entity entity;
    mScene->CreateEntity([&](auto ent, PositionComponent&, ModelComponent&) { entity = ent; },
                         PositionComponent { .x = 100 },
                         ModelComponent { .mesh = 200 });

    mScene->ExecuteTasks();

    // Assert
    auto has = mScene->TryGetComponents<PositionComponent, ModelComponent>(
        entity,
        [](PositionComponent& position, ModelComponent& model) {
            ASSERT_EQ(position.x, 100);
            ASSERT_EQ(model.mesh, 200);
        });
    ASSERT_TRUE(has);
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
    mScene->BeginProfiling();
    for (auto i = 0; i < 10; i++)
        mScene->Update(0.016f);
    mScene->EndProfiling();

    mScene->ExecuteTasks();

    auto count = mScene->Count<PositionComponent>();

    // Assert
    ASSERT_GT(count, 2000);
}

TEST_F(SceneSpec, SceneShouldBeDeterministicWhenIterating)
{
    // Arrange
    for (auto i = 0; i < 2000; i++)
    {
        mScene->CreateEntity(PositionComponent {});
    }

    // Act
    mScene->BeginProfiling();
    auto resultado = 0;
    for (auto i = 0; i < 1000; i++)
    {
        resultado += i;
        mScene->ForEach<PositionComponent>([i = i](auto, PositionComponent& position) { position.x += i; });
    }
    mScene->EndProfiling();

    auto count = mScene->Count<PositionComponent>();

    // Assert
    ASSERT_EQ(count, 2000);

    for (auto i = 0; i < 2000; i++)
    {
        auto has = mScene->TryGetComponents<PositionComponent>(i, [&](PositionComponent& position) {
            ASSERT_EQ(position.x, resultado);
        });
        ASSERT_TRUE(has);
    }
}

TEST_F(SceneSpec, SceneShouldBeDeterministicWhenRunningTasksInParallel)
{
    // Arrange
    for (auto i = 0; i < 2000; i++)
    {
        mScene->CreateEntity(PositionComponent {});
    }

    // Act
    mScene->BeginProfiling();
    auto resultado = 0;
    for (auto i = 0; i < 1000; i++)
    {
        resultado += i;
        mScene->ForEachAsync<PositionComponent>([i = i](auto, PositionComponent& position) { position.x += i; });
    }
    mScene->EndProfiling();

    mScene->ExecuteTasks();

    auto count = mScene->Count<PositionComponent>();

    // Assert
    ASSERT_EQ(count, 2000);

    for (auto i = 0; i < 2000; i++)
    {
        auto has = mScene->TryGetComponents<PositionComponent>(i, [&](PositionComponent& position) {
            ASSERT_EQ(position.x, resultado);
        });
        ASSERT_TRUE(has);
    }
}

TEST_F(SceneSpec, SceneShouldAddDeleteEntities)
{
    // Arrange
    auto entity = mScene->CreateEntity(PositionComponent { .x = 100 }, ModelComponent { .mesh = 200 });

    // Act
    mScene->DestroyEntity(entity);
    mScene->Update(0.016f);

    // Assert
    const auto has = mScene->HasComponents<PositionComponent, ModelComponent>(entity);
    ASSERT_FALSE(has);
}
TEST_F(SceneSpec, SceneShouldBeDestructedWhenAppFinish)
{
    // Arrange
    const std::weak_ptr scene          = mScene;
    const std::weak_ptr app            = mApp;
    const std::weak_ptr serviceProvide = mApp->GetRootServiceProvider().GetService<skr::ServiceProvider>();

    // Act
    mScene.reset();
    mApp.reset();

    // Assert
    ASSERT_TRUE(scene.expired());
    ASSERT_TRUE(app.expired());
}