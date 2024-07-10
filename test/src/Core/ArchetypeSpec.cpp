#include "gtest/gtest.h"

#include "../Components/PositionComponent.hpp"
#include <Freyr/Freyr.hpp>

class ArchetypeSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mArchetype = std::make_shared<fr::Archetype>(1000);
    }

    void TearDown() override { mArchetype.reset(); }

    std::shared_ptr<fr::Archetype> mArchetype;
};

TEST_F(ArchetypeSpec, ArchetypeShouldRegisterComponent)
{
    mArchetype->RegisterComponent<PositionComponent>();

    ASSERT_TRUE(mArchetype->HasComponent<PositionComponent>());
}

TEST_F(ArchetypeSpec, ArchetypeShouldBreakWhenAddingUnregisteredComponent)
{
    ASSERT_DEATH(mArchetype->AddComponent(0, PositionComponent {}),
                 "Component not registered before use.");
}

TEST_F(ArchetypeSpec, ArchetypeShouldGetCorrectComponent)
{
    mArchetype->RegisterComponent<PositionComponent>();
    mArchetype->AddComponent(0, PositionComponent {});
    mArchetype->GetComponent<PositionComponent>(0).x = 100;

    ASSERT_EQ(mArchetype->GetComponent<PositionComponent>(0).x, 100);
    ASSERT_DEATH(mArchetype->GetComponent<PositionComponent>(1).x,
                 "Retrieving non-existent component.");
}

TEST_F(ArchetypeSpec, ArchetypeShouldCannotGrow)
{
    mArchetype->RegisterComponent<PositionComponent>();

    for (auto i = 0; i < 2000; i++)
    {
        mArchetype->AddComponent(i, PositionComponent {});
    }
}