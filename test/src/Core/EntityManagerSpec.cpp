#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

class EntityManagerSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto app = skr::ApplicationBuilder().AddExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
            freyr.WithOptions([](fr::FreyrOptionsBuilder& builder) { builder.SetArchetypeChunkCapacity(1024); });
        });

        const auto provider = app.GetServiceCollection().CreateServiceProvider();

        mEntityManager = provider->GetService<fr::EntityManager>();
    }

    void TearDown() override { mEntityManager.reset(); }

    Ref<fr::EntityManager> mEntityManager;
};

TEST_F(EntityManagerSpec, EntityManagerShouldBeThreadSafeWhenCreatingEntities)
{
    constexpr auto threadCount       = 10;
    constexpr auto entitiesPerThread = 1'000;

    auto threads = std::vector<std::thread>();
    threads.reserve(threadCount);

    auto generatedEntities = fr::SparseSet<fr::Entity>(10'000);

    for (auto i = 0u; i < threadCount; ++i)
    {
        threads.emplace_back([&]() {
            for (auto j = 0u; j < entitiesPerThread; ++j)
                generatedEntities.insert(mEntityManager->CreateEntity());
        });
    }

    for (auto& thread : threads)
    {
        if (thread.joinable())
            thread.join();
    }

    ASSERT_EQ(generatedEntities.size(), threadCount * entitiesPerThread);
    ASSERT_EQ(mEntityManager->CreateEntity(), threadCount * entitiesPerThread);
}

TEST_F(EntityManagerSpec, EntityManagerShouldBeThreadSafeWhenDestroying)
{
    constexpr auto threadCount       = 10;
    constexpr auto entitiesPerThread = 1'000;

    auto threads = std::vector<std::thread>();
    threads.reserve(threadCount);

    auto generatedEntities = fr::SparseSet<fr::Entity>(10'000);

    for (auto i = 0u; i < threadCount; ++i)
    {
        threads.emplace_back([&]() {
            for (auto j = 0u; j < entitiesPerThread; ++j)
                generatedEntities.insert(mEntityManager->CreateEntity());
        });
    }

    for (auto& thread : threads)
    {
        if (thread.joinable())
            thread.join();
    }

    threads.clear();

    for (auto i = 0u; i < threadCount; ++i)
    {
        threads.emplace_back([&, threadId = i]() {
            for (auto j = 0u; j < entitiesPerThread; ++j)
            {
                generatedEntities.remove(threadId * entitiesPerThread + j);
                mEntityManager->DestroyEntity(threadId * entitiesPerThread + j);
            }
        });
    }

    for (auto& thread : threads)
    {
        if (thread.joinable())
            thread.join();
    }

    ASSERT_EQ(generatedEntities.size(), 0);
    ASSERT_EQ(mEntityManager->CreateEntity() % 1000, 0);
}
