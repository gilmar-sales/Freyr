#include "gtest/gtest.h"
#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/PositionComponent.hpp"

class SceneSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto app = skr::ApplicationBuilder().AddExtension<fr::FreyrExtension>(
            [](fr::FreyrExtension& freyr) {
                freyr.AddComponent<PositionComponent>()
                    .AddComponent<ModelComponent>()
                    .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                        builder.SetArchetypeChunkCapacity(2048);
                    });
            });
        const auto provider =
            app.GetServiceCollection().CreateServiceProvider();

        mScene = provider->GetService<fr::Scene>();
    }

    void TearDown() override { mScene.reset(); }

    Ref<fr::Scene> mScene;
};

TEST_F(SceneSpec, SceneShouldAddSingleComponent)
{
    // Arrange
    auto entity = mScene->CreateEntity();

    // Act
    mScene->AddComponent(entity, PositionComponent { .x = 100 });
    const auto& position = mScene->GetComponent<PositionComponent>(entity);

    // Assert
    ASSERT_EQ(position.x, 100);
}

TEST_F(SceneSpec, SceneShouldAddMultipleComponentsKeepingValues)
{
    // Arrange
    auto entity = mScene->CreateEntity();

    // Act
    mScene->AddComponent(entity, PositionComponent { .x = 100 });
    mScene->AddComponent(entity, ModelComponent { .mesh = 200 });

    const auto& position = mScene->GetComponent<PositionComponent>(entity);
    const auto& model    = mScene->GetComponent<ModelComponent>(entity);

    // Assert
    ASSERT_EQ(position.x, 100);
    ASSERT_EQ(model.mesh, 200);
}

TEST_F(SceneSpec, SceneShouldFindUnique)
{
    // Arrange
    for (auto i = 0; i < 2000; i++)
    {
        mScene->AddComponent(mScene->CreateEntity(), PositionComponent {});
    }

    const auto modelEntity = static_cast<fr::Entity>(987);
    mScene->AddComponent(modelEntity, ModelComponent {});

    // Act
    const auto unique = mScene->FindUnique<PositionComponent, ModelComponent>();

    // Assert
    ASSERT_EQ(unique, modelEntity);
}