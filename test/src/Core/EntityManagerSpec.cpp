#include "../EmptyApp.hpp"

#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

#include <algorithm>

class EntityManagerSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto app = skr::ApplicationBuilder()
                       .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                           freyr.WithOptions([](fr::FreyrOptionsBuilder& builder) {
                               builder.WithArchetypeChunkCapacity(1024);
                           });
                       })
                       .Build<EmptyApp>();

        mServiceProvider = app->GetRootServiceProvider();

        mEntityManager = mServiceProvider->GetService<fr::EntityManager>();
    }

    void TearDown() override { mEntityManager.reset(); }

    skr::Arc<skr::ServiceProvider> mServiceProvider;
    skr::Arc<fr::EntityManager>    mEntityManager;
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

TEST_F(EntityManagerSpec, EntityManagerShouldRecycleAllDestroyedEntities)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithOptions([](fr::FreyrOptionsBuilder& builder) {
                           builder.WithMaxEntities(32).WithArchetypeChunkCapacity(16);
                       });
                   })
                   .Build<EmptyApp>();

    auto entityManager = app->GetRootServiceProvider()->GetService<fr::EntityManager>();

    std::vector<fr::Entity> entities;
    entities.reserve(32);
    for (std::uint32_t i = 0; i < 32; ++i)
        entities.push_back(entityManager->CreateEntity());

    for (auto entity : entities)
        entityManager->DestroyEntity(entity);

    std::vector<fr::Entity> recycled;
    recycled.reserve(32);
    for (std::uint32_t i = 0; i < 32; ++i)
        recycled.push_back(entityManager->CreateEntity());

    std::sort(entities.begin(), entities.end());
    std::sort(recycled.begin(), recycled.end());
    ASSERT_EQ(entities, recycled);
}

TEST_F(EntityManagerSpec, EntityManagerShouldAllocateIdsStrictlyBelowMaxEntities)
{
    constexpr std::uint32_t kMaxEntities = 16;

    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithOptions([](fr::FreyrOptionsBuilder& builder) {
                           builder.WithMaxEntities(kMaxEntities).WithArchetypeChunkCapacity(8);
                       });
                   })
                   .Build<EmptyApp>();

    auto entityManager = app->GetRootServiceProvider()->GetService<fr::EntityManager>();

    std::vector<fr::Entity> entities;
    entities.reserve(kMaxEntities);
    for (std::uint32_t i = 0; i < kMaxEntities; ++i)
        entities.push_back(entityManager->CreateEntity());

    ASSERT_EQ(entities.size(), kMaxEntities);
    for (auto entity : entities)
        ASSERT_LT(entity, kMaxEntities);

    std::sort(entities.begin(), entities.end());
    entities.erase(std::unique(entities.begin(), entities.end()), entities.end());
    ASSERT_EQ(entities.size(), kMaxEntities);
}
