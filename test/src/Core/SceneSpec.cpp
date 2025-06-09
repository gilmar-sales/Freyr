#include "gtest/gtest.h"
#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/PositionComponent.hpp"

class SceneSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto app = skr::ApplicationBuilder().AddExtension(
            fr::FreyrExtension()
                .AddComponent<PositionComponent>()
                .AddComponent<ModelComponent>()
                .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                    builder.SetArchetypeChunkCapacity(2048);
                }));

        const auto provider =
            app.GetServiceCollection().CreateServiceProvider();

        mScene = provider->GetService<fr::Scene>();
    }

    void TearDown() override { mScene.reset(); }

    Ref<fr::Scene> mScene;
};

TEST_F(SceneSpec, ArchetypeGetUnique)
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