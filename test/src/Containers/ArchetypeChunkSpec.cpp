#include "../EmptyApp.hpp"

#include "gtest/gtest.h"
#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/NameComponent.hpp"
#include "../Components/PositionComponent.hpp"

class ArchetypeChunkSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mApp = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                       freyr.WithOptions([](fr::FreyrOptionsBuilder& builder) {
                           builder.WithArchetypeChunkCapacity(2048);
                       });
                   })
                   .Build<EmptyApp>();

        const auto provider = mApp->GetRootServiceProvider();

        mFreyrOptions                         = skr::MakeArc<fr::FreyrOptions>();
        mFreyrOptions->ArchetypeChunkCapacity = 2048;

        mThreadPool  = provider->GetService<fr::ThreadPool>();
        mTaskCounter = provider->GetService<fr::TaskCounter>();

        mInternalName = "TestArchetype";

        mArchetypeChunk = skr::MakeArc<fr::ArchetypeChunk>(
            mInternalName,
            mFreyrOptions,
            mThreadPool,
            mTaskCounter);
    }

    void TearDown() override
    {
        mArchetypeChunk.reset();
        mTaskCounter.reset();
        mThreadPool.reset();
        mFreyrOptions.reset();
        mApp.reset();
    }

    skr::Arc<fr::ArchetypeChunk> MakeChunk()
    {
        return skr::MakeArc<fr::ArchetypeChunk>(
            "OtherChunk",
            mFreyrOptions,
            mThreadPool,
            mTaskCounter);
    }

    skr::Arc<EmptyApp>           mApp;
    skr::Arc<fr::ArchetypeChunk> mArchetypeChunk;
    skr::Arc<fr::FreyrOptions>   mFreyrOptions;
    skr::Arc<fr::ThreadPool>     mThreadPool;
    skr::Arc<fr::TaskCounter>    mTaskCounter;
    std::string                  mInternalName;
};

TEST_F(ArchetypeChunkSpec, TryAddEntity_ShouldReturnTrue_WhenChunkNotFull)
{
    fr::Entity entity = 1;

    bool result = mArchetypeChunk->TryAddEntity(entity);

    EXPECT_TRUE(result);
    EXPECT_EQ(mArchetypeChunk->Count(), 1);
}

TEST_F(ArchetypeChunkSpec, TryAddEntity_ShouldReturnFalse_WhenChunkIsFull)
{
    for (size_t i = 0; i < mFreyrOptions->ArchetypeChunkCapacity; ++i)
    {
        EXPECT_TRUE(mArchetypeChunk->TryAddEntity(static_cast<fr::Entity>(i)));
    }

    fr::Entity extraEntity = mFreyrOptions->ArchetypeChunkCapacity;
    bool       result      = mArchetypeChunk->TryAddEntity(extraEntity);

    EXPECT_FALSE(result);
    EXPECT_EQ(mArchetypeChunk->Count(), mFreyrOptions->ArchetypeChunkCapacity);
}

TEST_F(ArchetypeChunkSpec, TryAddEntity_ShouldAddMultipleEntities)
{
    EXPECT_TRUE(mArchetypeChunk->TryAddEntity(1));
    EXPECT_TRUE(mArchetypeChunk->TryAddEntity(2));
    EXPECT_TRUE(mArchetypeChunk->TryAddEntity(3));

    EXPECT_EQ(mArchetypeChunk->Count(), 3);
}

TEST_F(ArchetypeChunkSpec, RemoveEntity_ShouldRemoveEntity)
{
    mArchetypeChunk->TryAddEntity(1);
    mArchetypeChunk->RemoveEntity(1);

    EXPECT_EQ(mArchetypeChunk->Count(), 0);
}

TEST_F(ArchetypeChunkSpec, RemoveEntity_ShouldMaintainCorrectCountWithMultipleEntities)
{
    mArchetypeChunk->TryAddEntity(1);
    mArchetypeChunk->TryAddEntity(2);
    mArchetypeChunk->TryAddEntity(3);

    mArchetypeChunk->RemoveEntity(2);

    EXPECT_EQ(mArchetypeChunk->Count(), 2);
}

TEST_F(ArchetypeChunkSpec, AddComponentArray_ShouldRegisterComponentType)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();

    fr::Entity entity = 1;
    mArchetypeChunk->TryAddEntity(entity);

    PositionComponent pos = { .x = 1.0f, .y = 2.0f, .z = 3.0f };
    mArchetypeChunk->AddComponent(entity, pos);

    auto& retrievedPos = mArchetypeChunk->GetComponent<PositionComponent>(entity);
    EXPECT_FLOAT_EQ(retrievedPos.x, 1.0f);
    EXPECT_FLOAT_EQ(retrievedPos.y, 2.0f);
    EXPECT_FLOAT_EQ(retrievedPos.z, 3.0f);
}

TEST_F(ArchetypeChunkSpec, AddComponentArray_ShouldIgnoreDuplicateRegistration)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();
    mArchetypeChunk->AddComponentArray<PositionComponent>();

    mArchetypeChunk->TryAddEntity(1);
    mArchetypeChunk->AddComponent(1, PositionComponent { .x = 4.f, .y = 5.f, .z = 6.f });

    EXPECT_FLOAT_EQ(mArchetypeChunk->GetComponent<PositionComponent>(1).x, 4.f);
}

TEST_F(ArchetypeChunkSpec, AddComponent_ShouldStoreComponentData)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();

    fr::Entity entity = 42;
    mArchetypeChunk->TryAddEntity(entity);

    PositionComponent pos = { .x = 10.0f, .y = 20.0f, .z = 30.0f };
    mArchetypeChunk->AddComponent(entity, pos);

    auto& storedPos = mArchetypeChunk->GetComponent<PositionComponent>(entity);
    EXPECT_FLOAT_EQ(storedPos.x, 10.0f);
    EXPECT_FLOAT_EQ(storedPos.y, 20.0f);
    EXPECT_FLOAT_EQ(storedPos.z, 30.0f);
}

TEST_F(ArchetypeChunkSpec, GetComponent_ShouldReturnCorrectComponent)
{
    mArchetypeChunk->AddComponentArray<ModelComponent>();

    fr::Entity entity = 5;
    mArchetypeChunk->TryAddEntity(entity);

    ModelComponent model;
    model.mesh = 123;
    mArchetypeChunk->AddComponent(entity, model);

    auto& retrievedModel = mArchetypeChunk->GetComponent<ModelComponent>(entity);
    EXPECT_EQ(retrievedModel.mesh, 123);
}

TEST_F(ArchetypeChunkSpec, GetComponents_ShouldReturnMultipleComponents)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();
    mArchetypeChunk->AddComponentArray<ModelComponent>();

    fr::Entity entity = 10;
    mArchetypeChunk->TryAddEntity(entity);

    PositionComponent pos = { .x = 5.0f, .y = 10.0f, .z = 15.0f };
    ModelComponent    model;
    model.mesh = 999;

    mArchetypeChunk->AddComponent(entity, pos);
    mArchetypeChunk->AddComponent(entity, model);

    auto [retrievedPos, retrievedModel] =
        mArchetypeChunk->GetComponents<PositionComponent, ModelComponent>(entity);

    EXPECT_FLOAT_EQ(retrievedPos.x, 5.0f);
    EXPECT_EQ(retrievedModel.mesh, 999);
}

TEST_F(ArchetypeChunkSpec, RemoveComponent_ShouldSwapWithLastComponentSlot)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();

    mArchetypeChunk->TryAddEntity(1);
    mArchetypeChunk->TryAddEntity(2);
    mArchetypeChunk->AddComponent(1, PositionComponent { .x = 1.f, .y = 0.f, .z = 0.f });
    mArchetypeChunk->AddComponent(2, PositionComponent { .x = 2.f, .y = 0.f, .z = 0.f });

    mArchetypeChunk->RemoveComponent<PositionComponent>(1);

    EXPECT_FLOAT_EQ(mArchetypeChunk->GetComponent<PositionComponent>(2).x, 2.f);
}

TEST_F(ArchetypeChunkSpec, AddComponents_ShouldApplyValuesAndInvokeCallback)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();
    mArchetypeChunk->AddComponentArray<ModelComponent>();

    constexpr fr::Entity entity = 11;
    mArchetypeChunk->TryAddEntity(entity);

    mThreadPool->StartWorkers();

    bool callbackRan = false;
    mArchetypeChunk->AddComponents<PositionComponent, ModelComponent>(
        entity,
        PositionComponent { .x = 8.f, .y = 9.f, .z = 10.f },
        ModelComponent { .mesh = 77 },
        [&](fr::Entity e, PositionComponent& position, ModelComponent& model) {
            callbackRan = true;
            EXPECT_EQ(e, entity);
            EXPECT_FLOAT_EQ(position.x, 8.f);
            EXPECT_EQ(model.mesh, 77);
        });

    mThreadPool->WaitForAllTasks();

    EXPECT_TRUE(callbackRan);
    EXPECT_FLOAT_EQ(mArchetypeChunk->GetComponent<PositionComponent>(entity).y, 9.f);
    EXPECT_EQ(mArchetypeChunk->GetComponent<ModelComponent>(entity).mesh, 77);
}

TEST_F(ArchetypeChunkSpec, ForEach_ShouldIterateOverAllEntities)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();

    std::vector<fr::Entity> entities = { 1, 2, 3, 4, 5 };
    for (auto entity : entities)
    {
        mArchetypeChunk->TryAddEntity(entity);
        PositionComponent pos = { .x = static_cast<float>(entity), .y = 0.0f, .z = 0.0f };
        mArchetypeChunk->AddComponent(entity, pos);
    }

    int callCount = 0;
    mArchetypeChunk->ForEach<PositionComponent>(
        "TestIteration",
        [&callCount](fr::Entity entity, PositionComponent&) {
            callCount++;
            EXPECT_GT(entity, 0);
        });

    EXPECT_EQ(callCount, 5);
}

TEST_F(ArchetypeChunkSpec, ForEach_ShouldProvideCorrectComponentData)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();

    fr::Entity entity1 = 1;
    fr::Entity entity2 = 2;

    mArchetypeChunk->TryAddEntity(entity1);
    mArchetypeChunk->TryAddEntity(entity2);

    PositionComponent pos1 = { .x = 10.0f, .y = 20.0f, .z = 30.0f };
    PositionComponent pos2 = { .x = 40.0f, .y = 50.0f, .z = 60.0f };

    mArchetypeChunk->AddComponent(entity1, pos1);
    mArchetypeChunk->AddComponent(entity2, pos2);

    std::map<fr::Entity, PositionComponent> results;
    mArchetypeChunk->ForEach<PositionComponent>(
        "TestData",
        [&results](fr::Entity entity, PositionComponent& pos) { results[entity] = pos; });

    EXPECT_FLOAT_EQ(results[entity1].x, 10.0f);
    EXPECT_FLOAT_EQ(results[entity2].x, 40.0f);
}

TEST_F(ArchetypeChunkSpec, ForEach_WithoutEntity_ShouldIterateComponents)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();

    mArchetypeChunk->TryAddEntity(1);
    mArchetypeChunk->TryAddEntity(2);
    mArchetypeChunk->AddComponent(1, PositionComponent { .x = 1.f, .y = 0.f, .z = 0.f });
    mArchetypeChunk->AddComponent(2, PositionComponent { .x = 2.f, .y = 0.f, .z = 0.f });

    float total = 0.f;
    mArchetypeChunk->ForEach<PositionComponent>("NoEntity", [&total](PositionComponent& position) {
        total += position.x;
    });

    EXPECT_FLOAT_EQ(total, 3.f);
}

TEST_F(ArchetypeChunkSpec, ForEach_WithEntityFilter_ShouldSkipMissingEntities)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();

    mArchetypeChunk->TryAddEntity(1);
    mArchetypeChunk->TryAddEntity(2);
    mArchetypeChunk->AddComponent(1, PositionComponent { .x = 10.f, .y = 0.f, .z = 0.f });
    mArchetypeChunk->AddComponent(2, PositionComponent { .x = 20.f, .y = 0.f, .z = 0.f });

    fr::SparseSet<fr::Entity> filter;
    filter.insert(2);
    filter.insert(99);

    float total = 0.f;
    int   calls = 0;
    mArchetypeChunk->ForEach<PositionComponent>(
        "Filtered",
        filter,
        [&](fr::Entity entity, PositionComponent& position) {
            ++calls;
            EXPECT_EQ(entity, 2);
            total += position.x;
        });

    EXPECT_EQ(calls, 1);
    EXPECT_FLOAT_EQ(total, 20.f);
}

TEST_F(ArchetypeChunkSpec, ForEachAsync_ShouldRunAfterTasksComplete)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();

    mArchetypeChunk->TryAddEntity(1);
    mArchetypeChunk->AddComponent(1, PositionComponent { .x = 5.f, .y = 0.f, .z = 0.f });

    mThreadPool->StartWorkers();

    std::atomic<float> seenX { 0.f };
    mArchetypeChunk->ForEachAsync<PositionComponent>(
        "Async",
        [&seenX](fr::Entity, PositionComponent& position) { seenX.store(position.x); });

    mThreadPool->WaitForAllTasks();

    EXPECT_FLOAT_EQ(seenX.load(), 5.f);
}

TEST_F(ArchetypeChunkSpec, Map_ShouldWriteTransformedValuesIntoBuffer)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();

    mArchetypeChunk->TryAddEntity(1);
    mArchetypeChunk->TryAddEntity(2);
    mArchetypeChunk->AddComponent(1, PositionComponent { .x = 3.f, .y = 0.f, .z = 0.f });
    mArchetypeChunk->AddComponent(2, PositionComponent { .x = 4.f, .y = 0.f, .z = 0.f });

    std::vector<float> buffer(2);
    mArchetypeChunk->Map<PositionComponent>(
        [](fr::Entity, PositionComponent& position) { return position.x; },
        0,
        buffer);

    EXPECT_FLOAT_EQ(buffer[0] + buffer[1], 7.f);
}

TEST_F(ArchetypeChunkSpec, IsFull_ShouldReturnFalse_WhenChunkNotFull)
{
    mArchetypeChunk->TryAddEntity(1);

    EXPECT_FALSE(mArchetypeChunk->IsFull());
}

TEST_F(ArchetypeChunkSpec, IsFull_ShouldReturnTrue_WhenChunkIsFull)
{
    for (size_t i = 0; i < mFreyrOptions->ArchetypeChunkCapacity; ++i)
    {
        mArchetypeChunk->TryAddEntity(static_cast<fr::Entity>(i));
    }

    EXPECT_TRUE(mArchetypeChunk->IsFull());
}

TEST_F(ArchetypeChunkSpec, Count_ShouldReturnCorrectEntityCount)
{
    EXPECT_EQ(mArchetypeChunk->Count(), 0);

    mArchetypeChunk->TryAddEntity(1);
    EXPECT_EQ(mArchetypeChunk->Count(), 1);

    mArchetypeChunk->TryAddEntity(2);
    EXPECT_EQ(mArchetypeChunk->Count(), 2);

    mArchetypeChunk->RemoveEntity(1);
    EXPECT_EQ(mArchetypeChunk->Count(), 1);
}

TEST_F(ArchetypeChunkSpec, GetRegisteredEntities_ShouldReturnAllEntities)
{
    std::vector<fr::Entity> expectedEntities = { 1, 2, 3, 4, 5 };

    for (auto entity : expectedEntities)
    {
        mArchetypeChunk->TryAddEntity(entity);
    }

    std::vector<std::uint32_t> registeredEntities;
    mArchetypeChunk->GetRegisteredEntities(registeredEntities);

    EXPECT_EQ(registeredEntities.size(), expectedEntities.size());

    for (auto entity : expectedEntities)
    {
        EXPECT_TRUE(std::find(registeredEntities.begin(), registeredEntities.end(), entity) !=
                    registeredEntities.end());
    }
}

TEST_F(ArchetypeChunkSpec, Swap_ShouldSwapEntityComponents)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();

    fr::Entity entity1 = 1;
    fr::Entity entity2 = 2;

    mArchetypeChunk->TryAddEntity(entity1);
    mArchetypeChunk->TryAddEntity(entity2);

    PositionComponent pos1 = { .x = 10.0f, .y = 20.0f, .z = 30.0f };
    PositionComponent pos2 = { .x = 40.0f, .y = 50.0f, .z = 60.0f };

    mArchetypeChunk->AddComponent(entity1, pos1);
    mArchetypeChunk->AddComponent(entity2, pos2);

    mArchetypeChunk->Swap(entity1, entity2);

    auto& swappedPos1 = mArchetypeChunk->GetComponent<PositionComponent>(entity1);
    auto& swappedPos2 = mArchetypeChunk->GetComponent<PositionComponent>(entity2);

    EXPECT_FLOAT_EQ(swappedPos1.x, 40.0f);
    EXPECT_FLOAT_EQ(swappedPos2.x, 10.0f);
}

TEST_F(ArchetypeChunkSpec, CopyEntity_ShouldCopySharedComponentsToTargetChunk)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();

    auto target = MakeChunk();
    target->AddComponentArray<PositionComponent>();

    mArchetypeChunk->TryAddEntity(1);
    target->TryAddEntity(2);

    mArchetypeChunk->AddComponent(1, PositionComponent { .x = 12.f, .y = 13.f, .z = 14.f });
    target->AddComponent(2, PositionComponent {});

    mArchetypeChunk->CopyEntity(1, 2, target.get());

    EXPECT_FLOAT_EQ(target->GetComponent<PositionComponent>(2).x, 12.f);
    EXPECT_FLOAT_EQ(target->GetComponent<PositionComponent>(2).y, 13.f);
    EXPECT_FLOAT_EQ(mArchetypeChunk->GetComponent<PositionComponent>(1).x, 12.f);
}

TEST_F(ArchetypeChunkSpec, MoveData_ShouldCopyMatchingComponentsAndRemoveSourceEntity)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();
    mArchetypeChunk->AddComponentArray<ModelComponent>();

    auto target = MakeChunk();
    target->AddComponentArray<PositionComponent>();

    mArchetypeChunk->TryAddEntity(7);
    target->TryAddEntity(7);

    mArchetypeChunk->AddComponent(7, PositionComponent { .x = 1.f, .y = 2.f, .z = 3.f });
    mArchetypeChunk->AddComponent(7, ModelComponent { .mesh = 55 });
    target->AddComponent(7, PositionComponent {});

    mArchetypeChunk->MoveData(7, target.get());

    EXPECT_EQ(mArchetypeChunk->Count(), 0);
    EXPECT_EQ(target->Count(), 1);
    EXPECT_FLOAT_EQ(target->GetComponent<PositionComponent>(7).x, 1.f);
    EXPECT_FLOAT_EQ(target->GetComponent<PositionComponent>(7).y, 2.f);
}

TEST_F(ArchetypeChunkSpec, RemoveEntity_ShouldSwapRemoveAcrossMultipleComponentArrays)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();
    mArchetypeChunk->AddComponentArray<ModelComponent>();
    mArchetypeChunk->AddComponentArray<NameComponent>();

    mArchetypeChunk->TryAddEntity(1);
    mArchetypeChunk->TryAddEntity(2);
    mArchetypeChunk->TryAddEntity(3);

    mArchetypeChunk->AddComponent(1, PositionComponent { .x = 1.f });
    mArchetypeChunk->AddComponent(1, ModelComponent { .mesh = 11 });
    mArchetypeChunk->AddComponent(1, NameComponent { .name = "a" });

    mArchetypeChunk->AddComponent(2, PositionComponent { .x = 2.f });
    mArchetypeChunk->AddComponent(2, ModelComponent { .mesh = 22 });
    mArchetypeChunk->AddComponent(2, NameComponent { .name = "b" });

    mArchetypeChunk->AddComponent(3, PositionComponent { .x = 3.f });
    mArchetypeChunk->AddComponent(3, ModelComponent { .mesh = 33 });
    mArchetypeChunk->AddComponent(3, NameComponent { .name = "c" });

    mArchetypeChunk->RemoveEntity(1);

    EXPECT_EQ(mArchetypeChunk->Count(), 2);
    EXPECT_FLOAT_EQ(mArchetypeChunk->GetComponent<PositionComponent>(3).x, 3.f);
    EXPECT_EQ(mArchetypeChunk->GetComponent<ModelComponent>(2).mesh, 22);
    EXPECT_EQ(mArchetypeChunk->GetComponent<NameComponent>(3).name, "c");
}

TEST_F(ArchetypeChunkSpec, NextTask_ShouldDispatchQueuedTaskToThreadPool)
{
    std::atomic<bool> ran { false };
    mArchetypeChunk->EnqueueTask([&ran] { ran.store(true); });

    mThreadPool->StartWorkers();
    mArchetypeChunk->NextTask();
    mThreadPool->WaitForAllTasks();

    EXPECT_TRUE(ran.load());
}

TEST_F(ArchetypeChunkSpec, NextTask_ShouldNoOpWhenQueueIsEmpty)
{
    mThreadPool->StartWorkers();
    mArchetypeChunk->NextTask();
    mThreadPool->WaitForAllTasks();

    EXPECT_EQ(mArchetypeChunk->Count(), 0);
}

TEST_F(ArchetypeChunkSpec, Destructor_ShouldHandleChunkWithoutComponentArrays)
{
    auto chunk = MakeChunk();
    chunk.reset();
}

TEST_F(ArchetypeChunkSpec, EnqueueTask_WhileWorkersBusy_ShouldNotStartExtraDrainTask)
{
    mThreadPool->StartWorkers();

    std::atomic<int>  hits { 0 };
    std::atomic<bool> release { false };

    mArchetypeChunk->EnqueueTask([&] {
        hits.fetch_add(1);
        while (!release.load())
            std::this_thread::yield();
    });

    while (hits.load() == 0)
        std::this_thread::yield();

    mArchetypeChunk->EnqueueTask([&] { hits.fetch_add(1); });
    release.store(true);
    mThreadPool->WaitForAllTasks();

    EXPECT_EQ(hits.load(), 2);
}

TEST_F(ArchetypeChunkSpec, StartTasks_ShouldDrainQueuedTasks)
{
    std::atomic<int> hits { 0 };
    mArchetypeChunk->EnqueueTask([&hits] { hits.fetch_add(1); });
    mArchetypeChunk->EnqueueTask([&hits] { hits.fetch_add(1); });

    mThreadPool->StartWorkers();
    mArchetypeChunk->StartTasks();
    mThreadPool->WaitForAllTasks();

    EXPECT_EQ(hits.load(), 2);
}

TEST_F(ArchetypeChunkSpec, StartTasks_WhileDrainActive_ShouldNotStartExtraDrainTask)
{
    mThreadPool->StartWorkers();

    std::atomic<int>  hits { 0 };
    std::atomic<bool> release { false };
    std::atomic<int>  concurrentDrainers { 0 };
    std::atomic<int>  maxConcurrentDrainers { 0 };

    mArchetypeChunk->EnqueueTask([&] {
        const int active   = concurrentDrainers.fetch_add(1) + 1;
        int       observed = maxConcurrentDrainers.load();
        while (active > observed && !maxConcurrentDrainers.compare_exchange_weak(observed, active))
        {
        }

        hits.fetch_add(1);
        while (!release.load())
            std::this_thread::yield();

        concurrentDrainers.fetch_sub(1);
    });

    while (hits.load() == 0)
        std::this_thread::yield();

    mArchetypeChunk->StartTasks();
    mArchetypeChunk->EnqueueTask([&] {
        const int active   = concurrentDrainers.fetch_add(1) + 1;
        int       observed = maxConcurrentDrainers.load();
        while (active > observed && !maxConcurrentDrainers.compare_exchange_weak(observed, active))
        {
        }

        hits.fetch_add(1);
        concurrentDrainers.fetch_sub(1);
    });

    release.store(true);
    mThreadPool->WaitForAllTasks();

    EXPECT_EQ(hits.load(), 2);
    EXPECT_EQ(maxConcurrentDrainers.load(), 1);
}

TEST_F(ArchetypeChunkSpec, MultipleComponentTypes_ShouldWorkCorrectly)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();
    mArchetypeChunk->AddComponentArray<ModelComponent>();

    fr::Entity entity = 100;
    mArchetypeChunk->TryAddEntity(entity);

    PositionComponent pos = { .x = 7.0f, .y = 8.0f, .z = 9.0f };
    ModelComponent    model;
    model.mesh = 456;

    mArchetypeChunk->AddComponent(entity, pos);
    mArchetypeChunk->AddComponent(entity, model);

    auto& retrievedPos   = mArchetypeChunk->GetComponent<PositionComponent>(entity);
    auto& retrievedModel = mArchetypeChunk->GetComponent<ModelComponent>(entity);

    EXPECT_FLOAT_EQ(retrievedPos.x, 7.0f);
    EXPECT_EQ(retrievedModel.mesh, 456);
}

TEST_F(ArchetypeChunkSpec, EmptyChunk_ShouldHandleOperationsGracefully)
{
    EXPECT_EQ(mArchetypeChunk->Count(), 0);
    EXPECT_FALSE(mArchetypeChunk->IsFull());

    std::vector<std::uint32_t> entities;
    mArchetypeChunk->GetRegisteredEntities(entities);
    EXPECT_TRUE(entities.empty());
}

TEST_F(ArchetypeChunkSpec, AddSameEntityTwice_ShouldHandleCorrectly)
{
    fr::Entity entity = 42;

    EXPECT_TRUE(mArchetypeChunk->TryAddEntity(entity));

    mArchetypeChunk->TryAddEntity(entity);

    EXPECT_LE(mArchetypeChunk->Count(), 1);
}
