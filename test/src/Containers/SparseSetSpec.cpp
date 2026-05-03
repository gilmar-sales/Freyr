#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"

class LockingSparseSetSpec : public ::testing::Test
{
    void SetUp() override { mFreyrOptions = skr::MakeRef<fr::FreyrOptions>(); }

  protected:
    Ref<fr::FreyrOptions> mFreyrOptions;
};

TEST_F(LockingSparseSetSpec, LockingSparseSetShouldSupportPointers)
{
    // Arrange
    auto componentArrays = fr::LockingSparseSet<fr::IComponentArray*>();

    // Act
    auto modelArray = new fr::ComponentArray<ModelComponent>(mFreyrOptions->ArchetypeChunkCapacity);
    componentArrays.insert(modelArray);

    // Assert
    auto modelId = fr::GetComponentId<ModelComponent>();
    ASSERT_EQ(componentArrays[modelId], modelArray);
}

TEST_F(LockingSparseSetSpec, LockingSparseSetShouldResetSizeAfterClean)
{
    // Arrange
    auto set = fr::LockingSparseSet<fr::Entity>();

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

TEST_F(LockingSparseSetSpec, LockingSparseSetShouldBeThreadSafeWhenCreatingEntities)
{
    // Arrange
    constexpr auto threadCount       = 8;
    constexpr auto entitiesPerThread = 3'000;
    auto           generatedEntities = fr::LockingSparseSet<fr::Entity>(entitiesPerThread * threadCount);

    // Act
    auto threads = std::vector<std::thread>();

    for (auto i = 0u; i < threadCount; ++i)
    {
        threads.emplace_back([i = i, &generatedEntities]() {
            for (auto j = 0u; j < entitiesPerThread; ++j)
                generatedEntities.insert(i * entitiesPerThread + j);
        });
    }

    for (auto& thread : threads)
    {
        if (thread.joinable())
            thread.join();
    }

    // Assert
    ASSERT_EQ(generatedEntities.size(), threadCount * entitiesPerThread);
}

TEST_F(LockingSparseSetSpec, LockingSparseSetShouldSwapValuesPosition)
{
    // Arrange
    auto componentArrays = fr::LockingSparseSet<fr::IComponentArray*>();

    auto modelArray = new fr::ComponentArray<ModelComponent>(mFreyrOptions->ArchetypeChunkCapacity);
    auto nameArray  = new fr::ComponentArray<NameComponent>(mFreyrOptions->ArchetypeChunkCapacity);
    componentArrays.insert(modelArray);
    componentArrays.insert(nameArray);

    auto modelPos = componentArrays.getIndex(modelArray->GetComponentId()) ;
    auto namePos  = componentArrays.getIndex(nameArray->GetComponentId());

    // Act
    auto positionArray = new fr::ComponentArray<PositionComponent>(mFreyrOptions->ArchetypeChunkCapacity);

    componentArrays.swap(modelArray, positionArray);

    // Assert
    auto positionPos = componentArrays.getIndex(positionArray->GetComponentId());
    ASSERT_EQ(modelPos, positionPos);
}