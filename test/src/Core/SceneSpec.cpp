#include "gtest/gtest.h"
#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/PositionComponent.hpp"

class SceneSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        const auto serviceCollection = skr::MakeRef<skr::ServiceCollection>();
        const auto provider = serviceCollection->CreateServiceProvider();

        mScene = fr::SceneBuilder(serviceCollection)
                     .AddComponent<PositionComponent>()
                     .AddComponent<ModelComponent>()
                     .WithOptions([](fr::FreyrOptionsBuilder& builder) {
                         builder.SetArchetypeChunkCapacity(2048);
                     })
                     .Build(*provider);
    }

    void TearDown() override { mScene.reset(); }

    std::shared_ptr<fr::Scene> mScene;
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