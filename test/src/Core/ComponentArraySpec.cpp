#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>
#include "../Components/PositionComponent.hpp"

class ComponentArraySpec
: public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mComponentArray = new fr::ComponentArray<PositionComponent>(1024);
    }

    void TearDown() override
    {
        delete mComponentArray;
    }

    fr::ComponentArray<PositionComponent>* mComponentArray;
};

TEST_F(ComponentArraySpec, ComponentArrayShouldCreateEntitiesModifyDataAndCopy) {
    for (int i =0; i < 100; i++)
        mComponentArray->AddEntity(i);
    ASSERT_EQ(mComponentArray->GetData(0).y, 0.0f);

    mComponentArray->GetData(0).y = 1.4f;
    ASSERT_EQ(mComponentArray->GetData(0).y, 1.4f);

    mComponentArray->CopyEntity(0, 50);
    ASSERT_EQ(mComponentArray->GetData(50).y, 1.4f);

    ASSERT_EQ(mComponentArray->Count(), 100);
}

TEST_F(ComponentArraySpec, ComponentArrayShouldCloneItsData) {
    for (int i =0; i < 100; i++)
    {
        mComponentArray->AddEntity(i);
        mComponentArray->GetData(i).y = static_cast<float>(i);
    }

    const auto newComponentArray = mComponentArray->Clone();

    mComponentArray->MoveData(newComponentArray);

    for (int i =0; i < 100; i++)
        ASSERT_EQ(mComponentArray->GetData(i).y, i);
}