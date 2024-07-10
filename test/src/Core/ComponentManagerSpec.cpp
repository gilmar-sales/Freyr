#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>
#include "../Components/PositionComponent.hpp"

class ComponentManagerSpec : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mComponentManager = new fr::ComponentManager(1000);
    }

    void TearDown() override
    {
        delete mComponentManager;
    }

    fr::ComponentManager* mComponentManager;
};


TEST_F(ComponentManagerSpec, ComponentManagerShouldAddEntities)
{
    for (auto i =0; i < 1200; i++)
        mComponentManager->AddComponent(0, PositionComponent{});

    ASSERT_EQ(mComponentManager->GetComponent<PositionComponent>(0).x, 0.0f);
}