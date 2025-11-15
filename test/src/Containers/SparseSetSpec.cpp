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

TEST_F(SparseSetSpec, SparseSetShouldResetSizeAfterClean)
{
    // Arrange
    auto set = fr::SparseSet<fr::Entity>();

    set.insert(1);
    set.insert(2);
    set.insert(3);

    // Act
    set.clear();

    // Assert
    ASSERT_EQ(set.size(), 0);
    ASSERT_FALSE(set.contains(1));
    ASSERT_FALSE(set.contains(2));
    ASSERT_FALSE(set.contains(3));
}

TEST_F(SparseSetSpec, SparseSetShouldBeThreadSafeWhenCreatingEntities)
{
    // Arrange
    constexpr auto threadCount       = 8;
    constexpr auto entitiesPerThread = 3'000;
    auto           generatedEntities = fr::SparseSet<fr::Entity>(entitiesPerThread * threadCount);

    // Act
    {
        auto threads = std::vector<std::jthread>();

        for (auto i = 0u; i < threadCount; ++i)
        {
            threads.emplace_back([i = i, &generatedEntities]() {
                for (auto j = 0u; j < entitiesPerThread; ++j)
                    generatedEntities.insert(i * entitiesPerThread + j);
            });
        }
    }

    // Assert
    ASSERT_EQ(generatedEntities.size(), threadCount * entitiesPerThread);
}