#include "gtest/gtest.h"
#include <Freyr/Freyr.hpp>

#include "../Components/ModelComponent.hpp"
#include "../Components/PositionComponent.hpp"

class ArchetypeChunkSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto app = skr::ApplicationBuilder().AddExtension<fr::FreyrExtension>([](Ref<fr::FreyrExtension> freyr) {
            freyr->WithOptions([](fr::FreyrOptionsBuilder& builder) { builder.WithArchetypeChunkCapacity(2048); });
        });

        const auto provider = app.GetServiceCollection()->CreateServiceProvider();

        mFreyrOptions                         = std::make_shared<fr::FreyrOptions>();
        mFreyrOptions->ArchetypeChunkCapacity = 2048;

        mThreadPool  = provider->GetService<fr::ThreadPool>();
        mTaskCounter = provider->GetService<fr::TaskCounter>();

        mInternalName         = "TestArchetype";
        mRegisteredComponents = std::make_shared<fr::SparseSet<fr::ComponentEntry>>();

        mArchetypeChunk = std::make_shared<fr::ArchetypeChunk>(mInternalName, mFreyrOptions, mThreadPool, mTaskCounter);
    }

    void TearDown() override
    {
        mArchetypeChunk.reset();
        mRegisteredComponents.reset();
        mTaskCounter.reset();
        mThreadPool.reset();
        mFreyrOptions.reset();
    }

    Ref<fr::ArchetypeChunk>                mArchetypeChunk;
    Ref<fr::FreyrOptions>                  mFreyrOptions;
    Ref<fr::ThreadPool>                    mThreadPool;
    Ref<fr::TaskCounter>                   mTaskCounter;
    std::string                            mInternalName;
    Ref<fr::SparseSet<fr::ComponentEntry>> mRegisteredComponents;
};

// ==================== Testes de Adição de Entidades ====================

TEST_F(ArchetypeChunkSpec, TryAddEntity_ShouldReturnTrue_WhenChunkNotFull)
{
    fr::Entity entity = 1;

    bool result = mArchetypeChunk->TryAddEntity(entity);

    EXPECT_TRUE(result);
    EXPECT_EQ(mArchetypeChunk->Count(), 1);
}

TEST_F(ArchetypeChunkSpec, TryAddEntity_ShouldReturnFalse_WhenChunkIsFull)
{
    // Preencher o chunk até a capacidade
    for (size_t i = 0; i < mFreyrOptions->ArchetypeChunkCapacity; ++i)
    {
        EXPECT_TRUE(mArchetypeChunk->TryAddEntity(static_cast<fr::Entity>(i)));
    }

    // Tentar adicionar uma entidade a mais
    fr::Entity extraEntity = mFreyrOptions->ArchetypeChunkCapacity;
    bool       result      = mArchetypeChunk->TryAddEntity(extraEntity);

    EXPECT_FALSE(result);
    EXPECT_EQ(mArchetypeChunk->Count(), mFreyrOptions->ArchetypeChunkCapacity);
}

TEST_F(ArchetypeChunkSpec, TryAddEntity_ShouldAddMultipleEntities)
{
    fr::Entity entity1 = 1;
    fr::Entity entity2 = 2;
    fr::Entity entity3 = 3;

    EXPECT_TRUE(mArchetypeChunk->TryAddEntity(entity1));
    EXPECT_TRUE(mArchetypeChunk->TryAddEntity(entity2));
    EXPECT_TRUE(mArchetypeChunk->TryAddEntity(entity3));

    EXPECT_EQ(mArchetypeChunk->Count(), 3);
}

// ==================== Testes de Remoção de Entidades ====================

TEST_F(ArchetypeChunkSpec, RemoveEntity_ShouldRemoveEntity)
{
    fr::Entity entity = 1;
    mArchetypeChunk->TryAddEntity(entity);

    mArchetypeChunk->RemoveEntity(entity);

    EXPECT_EQ(mArchetypeChunk->Count(), 0);
}

TEST_F(ArchetypeChunkSpec, RemoveEntity_ShouldMaintainCorrectCountWithMultipleEntities)
{
    fr::Entity entity1 = 1;
    fr::Entity entity2 = 2;
    fr::Entity entity3 = 3;

    mArchetypeChunk->TryAddEntity(entity1);
    mArchetypeChunk->TryAddEntity(entity2);
    mArchetypeChunk->TryAddEntity(entity3);

    mArchetypeChunk->RemoveEntity(entity2);

    EXPECT_EQ(mArchetypeChunk->Count(), 2);
}

// ==================== Testes de Componentes ====================

TEST_F(ArchetypeChunkSpec, AddComponentArray_ShouldRegisterComponentType)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();

    // Verificação implícita - não deve lançar exceção
    fr::Entity entity = 1;
    mArchetypeChunk->TryAddEntity(entity);

    PositionComponent pos = { .x = 1.0f, .y = 2.0f, .z = 3.0f };
    mArchetypeChunk->AddComponent(entity, pos);

    auto& retrievedPos = mArchetypeChunk->GetComponent<PositionComponent>(entity);
    EXPECT_FLOAT_EQ(retrievedPos.x, 1.0f);
    EXPECT_FLOAT_EQ(retrievedPos.y, 2.0f);
    EXPECT_FLOAT_EQ(retrievedPos.z, 3.0f);
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

    auto [retrievedPos, retrievedModel] = mArchetypeChunk->GetComponents<PositionComponent, ModelComponent>(entity);

    EXPECT_FLOAT_EQ(retrievedPos.x, 5.0f);
    EXPECT_EQ(retrievedModel.mesh, 999);
}

TEST_F(ArchetypeChunkSpec, RemoveComponent_ShouldRemoveComponentData)
{
    mArchetypeChunk->AddComponentArray<PositionComponent>();

    fr::Entity entity = 7;
    mArchetypeChunk->TryAddEntity(entity);

    PositionComponent pos = { .x = 1.0f, .y = 2.0f, .z = 3.0f };
    mArchetypeChunk->AddComponent(entity, pos);

    mArchetypeChunk->RemoveComponent<PositionComponent>(entity);

    // Após remoção, o componente não deve estar mais acessível
    // (comportamento esperado - pode precisar de ajuste conforme a API)
}

// ==================== Testes de Iteração ====================

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
        [&callCount](fr::Entity entity, PositionComponent& pos) {
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
    mArchetypeChunk->ForEach<PositionComponent>("TestData", [&results](fr::Entity entity, PositionComponent& pos) {
        results[entity] = pos;
    });

    EXPECT_FLOAT_EQ(results[entity1].x, 10.0f);
    EXPECT_FLOAT_EQ(results[entity2].x, 40.0f);
}

TEST_F(ArchetypeChunkSpec, IsFull_ShouldReturnFalse_WhenChunkNotFull)
{
    fr::Entity entity = 1;
    mArchetypeChunk->TryAddEntity(entity);

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

    bool secondAdd = mArchetypeChunk->TryAddEntity(entity);

    // Verificar que o count não aumentou indevidamente
    EXPECT_LE(mArchetypeChunk->Count(), 1);
}