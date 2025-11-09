#include "gtest/gtest.h"

#include "../Components/PositionComponent.hpp"
#include <Freyr/Freyr.hpp>

class ComponentArraySpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mComponentArray = new fr::ComponentArray<PositionComponent>(std::make_shared<fr::FreyrOptions>());
    }

    void TearDown() override { delete mComponentArray; }

    fr::ComponentArray<PositionComponent>* mComponentArray;
};

TEST_F(ComponentArraySpec, ComponentArrayShouldModifyDataAndCopy)
{
    ASSERT_EQ(mComponentArray->GetComponent(0).y, 0.0f);

    mComponentArray->GetComponent(0).y = 1.4f;
    ASSERT_EQ(mComponentArray->GetComponent(0).y, 1.4f);

    mComponentArray->CopyComponent(0, 50, nullptr);
    ASSERT_EQ(mComponentArray->GetComponent(50).y, 1.4f);
}
