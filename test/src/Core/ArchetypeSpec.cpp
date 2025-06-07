#include "gtest/gtest.h"
#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/PositionComponent.hpp"

class ArchetypeSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto app = skr::ApplicationBuilder().AddExtension(
            fr::FreyrExtension().WithOptions(
                [](fr::FreyrOptionsBuilder& builder) {
                    builder.SetArchetypeChunkCapacity(2048);
                }));

        const auto provider =
            app.GetServiceCollection().CreateServiceProvider();

        mArchetype = provider->GetService<fr::Archetype>();
    }

    void TearDown() override { mArchetype.reset(); }

    Ref<fr::Archetype> mArchetype;
};

TEST_F(ArchetypeSpec, ArchetypeShouldRegisterComponent)
{
    mArchetype->RegisterComponent<PositionComponent>();

    ASSERT_TRUE(mArchetype->HasComponent<PositionComponent>());
}

TEST_F(ArchetypeSpec, ArchetypeShouldBreakWhenAddingUnregisteredComponent)
{
    ASSERT_DEATH(mArchetype->AddComponent(0, PositionComponent {}), ".*");
}

TEST_F(ArchetypeSpec, ArchetypeShouldGetCorrectComponent)
{
    mArchetype->RegisterComponent<PositionComponent>();
    mArchetype->AddComponent(0, PositionComponent {});
    mArchetype->GetComponent<PositionComponent>(0).x = 100;

    ASSERT_EQ(mArchetype->GetComponent<PositionComponent>(0).x, 100);
    ASSERT_DEATH(mArchetype->GetComponent<PositionComponent>(1).x, ".*");
}

TEST_F(ArchetypeSpec, ArchetypeShouldCannotGrow)
{
    mArchetype->RegisterComponent<PositionComponent>();

    for (auto i = 0; i < 2000; i++)
    {
        mArchetype->AddComponent(i, PositionComponent {});
    }
}