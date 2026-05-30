
#include <Freyr/Freyr.hpp>
#include <gtest/gtest.h>

#include "../Components/DecayComponent.hpp"
#include "../Components/ModelComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "../EmptyApp.hpp"
#include "../Systems/DecaySystem.hpp"
#include "../Systems/MovementSystem.hpp"

class SceneSpec : public ::testing::TestWithParam<fr::FreyrOptions>
{
  protected:
    void SetUp() override
    {
        mApp = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithComponent<PositionComponent>()
                           .WithComponent<ModelComponent>()
                           .WithComponent<DecayComponent>()
                           .WithPipeline([](fr::PipelineBuilder& pipeline) {
                               pipeline.WithName("Physics").WithRate(0.02f).WithSystem<MovementSystem>();
                           });
                   })
                   .Build<EmptyApp>();

        mRegistry = mApp->GetRootServiceProvider()->GetService<fr::Registry>();
    }

    void TearDown() override
    {
        mRegistry.reset();
        mApp.reset();
    }

    Ref<fr::Registry> mRegistry;
    Ref<EmptyApp>       mApp;
};

TEST_F(SceneSpec, Scene_Should_TryGetSingleComponent)
{
    // Arrange
    mRegistry->CreateEntity([&](auto entity) {
        // Act
        mRegistry->AddComponent(entity, PositionComponent { .x = 100 });
        mRegistry->ExecuteTasks();

        // Assert
        ASSERT_TRUE(mRegistry->TryGetComponents<PositionComponent>(entity, [](PositionComponent& position) {
            ASSERT_EQ(position.x, 100);
        }));
    });
}

TEST_F(SceneSpec, Scene_Should_AddMultipleComponentsKeepingValues)
{
    // Arrange
    mRegistry->CreateEntity([&](auto entity) {
        // Act
        mRegistry->AddComponent(entity, PositionComponent { .x = 100 });
        mRegistry->AddComponent(entity, ModelComponent { .mesh = 200 });
        mRegistry->ExecuteTasks();

        // Assert
        auto has = mRegistry->TryGetComponents<PositionComponent, ModelComponent>(
            entity,
            [](const PositionComponent& position, const ModelComponent& model) {
                ASSERT_EQ(position.x, 100);
                ASSERT_EQ(model.mesh, 200);
            });
        ASSERT_TRUE(has);
    });
}

TEST_F(SceneSpec, Scene_Should_AddMultipleComponentsAtOnceKeepingValues)
{
    // Arrange
    fr::Entity entity = -1;

    // Act
    mRegistry->CreateEntity([&](auto ent, PositionComponent&, ModelComponent&) { entity = ent; },
                         PositionComponent { .x = 100 },
                         ModelComponent { .mesh = 200 });

    mRegistry->ExecuteTasks();

    // Assert
    ASSERT_NE(entity, -1);
    auto has = mRegistry->TryGetComponents<PositionComponent, ModelComponent>(
        entity,
        [](PositionComponent& position, ModelComponent& model) {
            ASSERT_EQ(position.x, 100);
            ASSERT_EQ(model.mesh, 200);
        });
    ASSERT_TRUE(has);
}

TEST_F(SceneSpec, Scene_Should_RemoveComponentKeepingValues)
{
    // Arrange
    fr::Entity entity = -1;

    mRegistry->CreateEntity([&](auto ent, PositionComponent&, ModelComponent&) { entity = ent; },
                         PositionComponent { .x = 100 },
                         ModelComponent { .mesh = 200 });

    mRegistry->ExecuteTasks();

    // Act
    mRegistry->RemoveComponent<ModelComponent>(entity);

    mRegistry->ExecuteTasks();

    // Assert
    auto hasPosition = mRegistry->TryGetComponents<PositionComponent>(entity, [](PositionComponent& position) {
        ASSERT_EQ(position.x, 100);
    });
    auto hasModel =
        mRegistry->TryGetComponents<ModelComponent>(entity, [](ModelComponent& model) { ASSERT_NE(model.mesh, 200); });
    ASSERT_NE(entity, -1);
    ASSERT_TRUE(hasPosition);
    ASSERT_FALSE(hasModel);
}

TEST_F(SceneSpec, Scene_Should_BeAbleToCreateAndDestroyEntitiesWhileUpdating)
{
    // Arrange
    for (auto i = 0; i < 2000; i++)
    {
        mRegistry->CreateEntity(PositionComponent {});
    }

    // Act
    for (auto i = 0; i < 10; i++)
        mRegistry->Update(0.016f);

    mRegistry->ExecuteTasks();

    const auto count = mRegistry->CreateQuery()->Count<PositionComponent>();

    // Assert
    ASSERT_GE(count, 2000);
}

TEST_F(SceneSpec, Scene_Should_BeDeterministicWhenIterating)
{
    // Arrange
    for (auto i = 0; i < 2000; i++)
    {
        mRegistry->CreateEntity(PositionComponent {});
    }

    // Act
    auto resultado = 0;
    for (auto i = 0; i < 1000; i++)
    {
        resultado += i;
        mRegistry->CreateQuery()->Each<PositionComponent>([i = i](auto, PositionComponent& position) { position.x += i; });
    }

    auto count = mRegistry->CreateQuery()->Count<PositionComponent>();

    // Assert
    ASSERT_EQ(count, 2000);

    for (auto i = 0; i < 2000; i++)
    {
        auto has = mRegistry->TryGetComponents<PositionComponent>(i, [&](PositionComponent& position) {
            ASSERT_EQ(position.x, resultado);
        });
        ASSERT_TRUE(has);
    }
}

TEST_F(SceneSpec, Scene_Should_BeDeterministicWhenRunningTasksInParallel)
{
    // Arrange
    for (auto i = 0; i < 2000; i++)
    {
        mRegistry->CreateEntity(PositionComponent {});
    }

    // Act
    auto resultado = 0;
    for (auto i = 0; i < 1000; i++)
    {
        resultado += i;
        mRegistry->CreateQuery()->EachAsync<PositionComponent>([i = i](auto, PositionComponent& position) {
            position.x += i;
        });
    }

    mRegistry->ExecuteTasks();

    auto count = mRegistry->CreateQuery()->Count<PositionComponent>();

    // Assert
    ASSERT_EQ(count, 2000);

    for (auto i = 0; i < 2000; i++)
    {
        auto has = mRegistry->TryGetComponents<PositionComponent>(i, [&](PositionComponent& position) {
            ASSERT_EQ(position.x, resultado);
        });
        ASSERT_TRUE(has);
    }

    mRegistry->ExecuteTasks();
}

TEST_F(SceneSpec, Scene_Should_AddDeleteEntities)
{
    // Arrange
    auto entity = mRegistry->CreateEntity(PositionComponent { .x = 100 }, ModelComponent { .mesh = 200 });

    // Act
    mRegistry->DestroyEntity(entity);
    mRegistry->Update(0.016f);

    // Assert
    const auto has = mRegistry->HasComponents<PositionComponent, ModelComponent>(entity);
    ASSERT_FALSE(has);
}

TEST_F(SceneSpec, Scene_Should_BeDestructedWhenAppFinish)
{
    // Arrange
    const std::weak_ptr scene          = mRegistry;
    const std::weak_ptr app            = mApp;
    const std::weak_ptr serviceProvide = mApp->GetRootServiceProvider()->GetService<skr::ServiceProvider>();

    // Act
    mRegistry.reset();
    mApp.reset();

    // Assert
    ASSERT_TRUE(scene.expired());
    ASSERT_TRUE(app.expired());
}