#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"

#include <vector>

class SparseSetSpec : public ::testing::Test
{
    void SetUp() override { mFreyrOptions = skr::MakeArc<fr::FreyrOptions>(); }

  protected:
    skr::Arc<fr::FreyrOptions> mFreyrOptions;
};

TEST_F(SparseSetSpec, SparseSetShouldSupportPointers)
{
    // Arrange
    auto componentArrays = fr::SparseSet<fr::IComponentArray*>();

    // Act
    auto modelArray = new fr::ComponentArray<ModelComponent>(mFreyrOptions->ArchetypeChunkCapacity);
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

TEST_F(SparseSetSpec, SparseSetShouldSwapValuesPosition)
{
    // Arrange
    auto componentArrays = fr::SparseSet<fr::IComponentArray*>();

    auto modelArray = new fr::ComponentArray<ModelComponent>(mFreyrOptions->ArchetypeChunkCapacity);
    auto nameArray  = new fr::ComponentArray<NameComponent>(mFreyrOptions->ArchetypeChunkCapacity);
    componentArrays.insert(modelArray);
    componentArrays.insert(nameArray);

    auto modelPos = componentArrays.getIndex(modelArray->GetComponentId());
    auto namePos  = componentArrays.getIndex(nameArray->GetComponentId());

    // Act
    auto positionArray =
        new fr::ComponentArray<PositionComponent>(mFreyrOptions->ArchetypeChunkCapacity);

    componentArrays.swap(modelArray, positionArray);

    // Assert
    auto positionPos = componentArrays.getIndex(positionArray->GetComponentId());
    ASSERT_EQ(modelPos, positionPos);
}

TEST_F(SparseSetSpec, SwapReturnsEarlyWhenFirstValueIsAbsent)
{
    auto set = fr::SparseSet<fr::Entity>();
    set.insert(2);

    set.swap(1, 3);

    EXPECT_TRUE(set.contains(2));
    EXPECT_FALSE(set.contains(3));
    EXPECT_EQ(set.size(), 1);
}

TEST_F(SparseSetSpec, SwapReturnsEarlyWhenSecondValueAlreadyPresent)
{
    auto set = fr::SparseSet<fr::Entity>();
    set.insert(1);
    set.insert(2);

    set.swap(1, 2);

    EXPECT_TRUE(set.contains(1));
    EXPECT_TRUE(set.contains(2));
    EXPECT_EQ(set.getIndex(1), 0);
    EXPECT_EQ(set.getIndex(2), 1);
}

TEST_F(SparseSetSpec, IterationFollowsInsertionOrder)
{
    auto set = fr::SparseSet<fr::Entity>();
    set.insert(10);
    set.insert(20);
    set.insert(30);

    std::vector<fr::Entity> iterated;
    for (auto entity : set)
        iterated.push_back(entity);

    ASSERT_EQ(iterated, (std::vector<fr::Entity> { 10, 20, 30 }));
}

TEST_F(SparseSetSpec, LocalSparseSetSupportsInsertLookupAndRemoveWithoutLocks)
{
    auto set = fr::LocalSparseSet<fr::Entity>(8);
    set.insert(3);
    set.insert(7);
    set.insert(11);

    ASSERT_EQ(set.size(), 3);
    ASSERT_TRUE(set.contains(7));
    ASSERT_EQ(set.getIndex(3), 0);
    ASSERT_EQ(set.getDense()[0], 3);

    set.remove(3);

    ASSERT_EQ(set.size(), 2);
    ASSERT_FALSE(set.contains(3));
    ASSERT_TRUE(set.contains(7));
    ASSERT_TRUE(set.contains(11));
}
TEST_F(SparseSetSpec, FindShouldReportAbsentKeysAfterRemoveSwapAndOnTouchedPages)
{
    auto set = fr::LocalSparseSet<fr::Entity>();
    set.insert(5);
    set.insert(9);
    set.insert(4'100);

    EXPECT_EQ(set.find(6), fr::LocalSparseSet<fr::Entity>::npos);
    EXPECT_EQ(set.find(4'099), fr::LocalSparseSet<fr::Entity>::npos);
    EXPECT_EQ(set.find(1'000'000), fr::LocalSparseSet<fr::Entity>::npos);
    EXPECT_EQ(set.find(5), 0u);
    EXPECT_EQ(set.index(9), 1u);

    set.remove(5);
    EXPECT_FALSE(set.contains(5));
    EXPECT_EQ(set.find(5), fr::LocalSparseSet<fr::Entity>::npos);
    EXPECT_EQ(set.index(4'100), 0u);
    EXPECT_EQ(set.index(9), 1u);

    set.swap(9, 12);
    EXPECT_EQ(set.find(9), fr::LocalSparseSet<fr::Entity>::npos);
    EXPECT_EQ(set.index(12), 1u);

    set.remove(12);
    set.remove(4'100);
    EXPECT_EQ(set.size(), 0u);
    EXPECT_FALSE(set.contains(4'100));
    EXPECT_FALSE(set.contains(0));

    set.insert(0);
    EXPECT_EQ(set.find(0), 0u);
    EXPECT_FALSE(set.contains(4'100));
}
