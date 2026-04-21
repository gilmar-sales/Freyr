#include <Freyr/Freyr.hpp>
#include <gtest/gtest.h>

#include "../Components/DecayComponent.hpp"
#include "../Components/ModelComponent.hpp"
#include "../Components/PositionComponent.hpp"

#include "../Systems/DecaySystem.hpp"
#include "../Systems/MovementSystem.hpp"

class App : public skr::IApplication
{
  public:
    explicit App(const Ref<skr::ServiceProvider>& rootServiceProvider) : IApplication(rootServiceProvider) {}

    void Run() override {}
};

inline fr::FreyrOptions ChunkAffinityConfig()
{
    return { .ExecutionStrategy = fr::FreyrExecutionStategy::ChunkAffinity };
}

inline fr::FreyrOptions DispatchOrderConfig()
{
    return { .ExecutionStrategy = fr::FreyrExecutionStategy::DispatchOrder };
}

class SceneSpec : public ::testing::TestWithParam<fr::FreyrOptions>
{
  protected:
    void SetUp() override
    {
        auto config = GetParam();
        mApp        = skr::ApplicationBuilder()
                          .AddExtension<fr::FreyrExtension>([&](const Ref<fr::FreyrExtension>& freyr) {
                       freyr->WithComponent<PositionComponent>()
                           .WithComponent<ModelComponent>()
                           .WithComponent<DecayComponent>()
                           .WithPipeline([](fr::PipelineBuilder& pipeline) { pipeline.WithSystem<DecaySystem>(); })
                           .WithPipeline([](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Physics").WithRate(0.02f).WithSystem<MovementSystem>();
                           })
                           .WithOptions([&](fr::FreyrOptionsBuilder& builder) {
                               builder.WithExecutionStrategy(config.ExecutionStrategy);
                           });
                          })
                          .Build<App>();

        mScene = mApp->GetRootServiceProvider()->GetService<fr::Scene>();
    }

    void TearDown() override
    {
        mScene.reset();
        mApp.reset();
    }

    Ref<fr::Scene> mScene;
    Ref<App>       mApp;
};

INSTANTIATE_TEST_SUITE_P(ChunkAffinity, SceneSpec, ::testing::Values(ChunkAffinityConfig()), [](const auto&) {
    return "ChunkAffinity";
});

INSTANTIATE_TEST_SUITE_P(DispatchOrder, SceneSpec, ::testing::Values(DispatchOrderConfig()), [](const auto&) {
    return "DispatchOrder";
});

TEST_P(SceneSpec, Scene_Should_TryGetSingleComponent)
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

TEST_P(SceneSpec, Scene_Should_AddMultipleComponentsKeepingValues)
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

TEST_P(SceneSpec, Scene_Should_AddMultipleComponentsAtOnceKeepingValues)
{
    // Arrange
    fr::Entity entity = -1;

    // Act
    mScene->CreateEntity([&](auto ent, PositionComponent&, ModelComponent&) { entity = ent; },
                         PositionComponent { .x = 100 },
                         ModelComponent { .mesh = 200 });

    mScene->ExecuteTasks();

    // Assert
    ASSERT_NE(entity, -1);
    auto has = mScene->TryGetComponents<PositionComponent, ModelComponent>(
        entity,
        [](PositionComponent& position, ModelComponent& model) {
            ASSERT_EQ(position.x, 100);
            ASSERT_EQ(model.mesh, 200);
        });
    ASSERT_TRUE(has);
}

TEST_P(SceneSpec, Scene_Should_RemoveComponentKeepingValues)
{
    // Arrange
    fr::Entity entity = -1;

    mScene->CreateEntity([&](auto ent, PositionComponent&, ModelComponent&) { entity = ent; },
                         PositionComponent { .x = 100 },
                         ModelComponent { .mesh = 200 });

    mScene->ExecuteTasks();

    // Act
    mScene->RemoveComponent<ModelComponent>(entity);

    mScene->ExecuteTasks();

    // Assert
    auto hasPosition = mScene->TryGetComponents<PositionComponent>(entity, [](PositionComponent& position) {
        ASSERT_EQ(position.x, 100);
    });
    auto hasModel =
        mScene->TryGetComponents<ModelComponent>(entity, [](ModelComponent& model) { ASSERT_NE(model.mesh, 200); });
    ASSERT_NE(entity, -1);
    ASSERT_TRUE(hasPosition);
    ASSERT_FALSE(hasModel);
}

TEST_P(SceneSpec, Scene_Should_FindUnique)
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

TEST_P(SceneSpec, Scene_Should_BeAbleToCreateAndDestroyEntitiesWhileUpdating)
{
    // Arrange
    for (auto i = 0; i < 2000; i++)
    {
        mScene->CreateEntity(PositionComponent {});
    }

    // Act
    for (auto i = 0; i < 10; i++)
        mScene->Update(0.016f);

    mScene->ExecuteTasks();

    auto count = mScene->Count<PositionComponent>();

    // Assert
    ASSERT_GT(count, 2000);
}

TEST_P(SceneSpec, Scene_Should_BeDeterministicWhenIterating)
{
    // Arrange
    for (auto i = 0; i < 2000; i++)
    {
        mScene->CreateEntity(PositionComponent {});
    }

    // Act
    auto resultado = 0;
    for (auto i = 0; i < 1000; i++)
    {
        resultado += i;
        mScene->ForEach<PositionComponent>([i = i](auto, PositionComponent& position) {
            position.x += static_cast<float>(i);
        });
    }

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

TEST_P(SceneSpec, Scene_Should_BeDeterministicWhenRunningTasksInParallel)
{
    // Arrange
    for (auto i = 0; i < 2000; i++)
    {
        mScene->CreateEntity(PositionComponent {});
    }

    // Act
    auto resultado = 0;
    for (auto i = 0; i < 1000; i++)
    {
        resultado += i;
        mScene->ForEachAsync<PositionComponent>([i = i](auto, PositionComponent& position) {
            position.x += static_cast<float>(i);
        });
    }

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

    mScene->ExecuteTasks();
}

TEST_P(SceneSpec, Scene_Should_AddDeleteEntities)
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

TEST_P(SceneSpec, Scene_Should_BeDestructedWhenAppFinish)
{
    // Arrange
    const std::weak_ptr scene          = mScene;
    const std::weak_ptr app            = mApp;
    const std::weak_ptr serviceProvide = mApp->GetRootServiceProvider()->GetService<skr::ServiceProvider>();

    // Act
    mScene.reset();
    mApp.reset();

    // Assert
    ASSERT_TRUE(scene.expired());
    ASSERT_TRUE(app.expired());
}
