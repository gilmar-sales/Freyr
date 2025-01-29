#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

class EntityManagerSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mEntityManager = std::make_shared<fr::EntityManager>(10'000);
    }

    void TearDown() override { mEntityManager.reset(); }

    std::shared_ptr<fr::EntityManager> mEntityManager;
};

TEST_F(EntityManagerSpec, EntityManagerShouldBeThreadSafe)
{
    constexpr auto threadCount       = 10;
    constexpr auto entitiesPerThread = 1'000;

    auto threads = std::vector<std::thread>();
    threads.reserve(threadCount);

    auto generatedEntities = fr::SparseSet<fr::Entity>(10'000);

    for (auto i = 0u; i < threadCount; ++i)
    {
        threads.push_back(std::thread { [&]() {
            for (auto j = 0u; j < entitiesPerThread; ++j)
                generatedEntities.insert(mEntityManager->CreateEntity());
        } });
    }

    for (auto& thread : threads)
    {
        if (thread.joinable())
            thread.join();
    }

    ASSERT_EQ(generatedEntities.size(), threadCount * entitiesPerThread);
    ASSERT_EQ(mEntityManager->CreateEntity(), threadCount * entitiesPerThread);
}