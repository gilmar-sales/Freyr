#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"

class SparseSetSpec : public ::testing::Test
{
    void SetUp() override { mFreyrOptions = skr::MakeRef<fr::FreyrOptions>(); }

  protected:
    Ref<fr::FreyrOptions> mFreyrOptions;
};

TEST_F(SparseSetSpec, SparseSetShouldSupportPointers)
{
    // Arrange
    auto componentArrays = fr::SparseSet<fr::IComponentArray*>();

    // Act
    auto modelArray = new fr::ComponentArray<ModelComponent>(mFreyrOptions);
    componentArrays.insert(modelArray);

    // Assert
    auto modelId = fr::GetComponentId<ModelComponent>();
    ASSERT_EQ(componentArrays[modelId], modelArray);
}